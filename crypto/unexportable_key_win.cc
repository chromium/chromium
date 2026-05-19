// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key_win.h"

#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_util_win.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_thread_priority.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/random.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"
#include "crypto/unexportable_key_metrics.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto {

namespace {

const char kMetricVirtualCreateKeyError[] = "Crypto.TpmError.VirtualCreateKey";
const char kMetricVirtualFinalizeKeyError[] =
    "Crypto.TpmError.VirtualFinalizeKey";
const char kMetricVirtualOpenKeyError[] = "Crypto.TpmError.VirtualOpenKey";
const char kMetricVirtualOpenStorageError[] =
    "Crypto.TpmError.VirtualOpenStorage";

enum class ProviderType {
  // Keys will be backed by a TPM. Requires TPM support.
  kTPM,

  // Keys will be backed by software. Widely available.
  kSoftware
};

// Identifies the purpose of the key to be generated.
enum class KeyUsage {
  // The key will be used for signing data (e.g. a session binding key).
  kSigning,
  // The key will be used as an attestation key (e.g. an AIK).
  kAttestation
};

// Holds the results of a successful key generation or loading.
struct KeyDetails {
  // The handle to the key.
  ScopedNCryptKey key;
  // The wrapped key blob that can be used to restore the key later.
  std::vector<uint8_t> wrapped_key;
  // The SubjectPublicKeyInfo for the public key.
  std::vector<uint8_t> spki;
  // The algorithm used for the key.
  SignatureVerifier::SignatureAlgorithm algo =
      SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
};

// WinKeyImpl shares common implementation for unexportable keys on Windows.
template <typename BaseInterface>
class WinKeyImpl : public BaseInterface {
 public:
  WinKeyImpl(ProviderType provider_type, KeyDetails details)
      : provider_type_(provider_type),
        key_(std::move(details.key)),
        wrapped_key_(std::move(details.wrapped_key)),
        spki_(std::move(details.spki)),
        algo_(details.algo) {}

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    return algo_;
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return spki_;
  }

  std::vector<uint8_t> GetWrappedKey() const override { return wrapped_key_; }

  bool IsHardwareBacked() const override {
    return provider_type_ == ProviderType::kTPM;
  }

 protected:
  const ProviderType provider_type_;
  ScopedNCryptKey key_;
  const std::vector<uint8_t> wrapped_key_;
  const std::vector<uint8_t> spki_;
  const SignatureVerifier::SignatureAlgorithm algo_;
};

LPCWSTR GetWindowsIdentifierForProvider(ProviderType type) {
  switch (type) {
    case ProviderType::kTPM:
      return MS_PLATFORM_CRYPTO_PROVIDER;
    case ProviderType::kSoftware:
      return MS_KEY_STORAGE_PROVIDER;
  }
}

std::u16string KeyIdToWindowsLabel(base::span<const uint8_t> key_id) {
  return u"unexportable-key-" + base::UTF8ToUTF16(base::Base64Encode(key_id));
}

// Logs `status` and `selected_algorithm` to an error histogram capturing that
// `operation` failed for a TPM-backed key.
void LogTPMOperationError(
    TPMOperation operation,
    SECURITY_STATUS status,
    std::optional<SignatureVerifier::SignatureAlgorithm> selected_algorithm,
    bool open_storage_provider_error = false) {
  static constexpr char kTPMOperationErrorHistogramFormat[] =
      "Crypto.TPMOperation.Win.%s%s.Error";
  // There are two cases that can be recorded without a `selected_algorithm`:
  //    1- OpenStorageProvider errors because these happen before an algorithm
  //       is chosen.
  //    2- Errors during `kWrappedKeyCreation` TPM operation.
  if (!open_storage_provider_error) {
    CHECK_EQ(!selected_algorithm.has_value(),
             operation == TPMOperation::kWrappedKeyCreation);
  }

  std::string algorithm_string =
      selected_algorithm ? AlgorithmToString(*selected_algorithm) : "";
  base::UmaHistogramSparse(
      base::StringPrintf(kTPMOperationErrorHistogramFormat,
                         OperationToString(operation).c_str(),
                         algorithm_string.c_str()),
      status);
}

std::vector<uint8_t> CBBToVector(const CBB* cbb) {
  return std::vector<uint8_t>(CBB_data(cbb),
                              UNSAFE_TODO(CBB_data(cbb) + CBB_len(cbb)));
}

// BCryptAlgorithmFor returns the BCrypt algorithm ID for the given Chromium
// signing algorithm.
std::optional<LPCWSTR> BCryptAlgorithmFor(
    SignatureVerifier::SignatureAlgorithm algo) {
  switch (algo) {
    case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      return BCRYPT_RSA_ALGORITHM;

    case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      return BCRYPT_ECDSA_P256_ALGORITHM;

    default:
      return std::nullopt;
  }
}

// GetBestSupported returns the first element of |acceptable_algorithms| that
// |provider| supports, or |nullopt| if there isn't any.
std::optional<SignatureVerifier::SignatureAlgorithm> GetBestSupported(
    NCRYPT_PROV_HANDLE provider,
    base::span<const SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms) {
  for (auto algo : acceptable_algorithms) {
    std::optional<LPCWSTR> bcrypto_algo_name = BCryptAlgorithmFor(algo);
    if (!bcrypto_algo_name) {
      continue;
    }

    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    SECURITY_STATUS status = NCryptIsAlgSupported(provider, *bcrypto_algo_name,
                                                  /*flags=*/0);
    if (FAILED(status)) {
      // `NTE_NOT_SUPPORTED` is expected when an algorithm is not supported.
      // Avoid recording it as an error as it may unnecessarily clutter the
      // metrics.
      //
      // https://learn.microsoft.com/en-us/windows/win32/api/ncrypt/nf-ncrypt-ncryptisalgsupported#return-value
      if (status != NTE_NOT_SUPPORTED) {
        LogTPMOperationError(TPMOperation::kSelectAlgorithm, status, algo);
      }
      continue;
    }
    return algo;
  }

  return std::nullopt;
}

// GetKeyProperty returns the given NCrypt key property of |key|.
std::optional<std::vector<uint8_t>> GetKeyProperty(NCRYPT_KEY_HANDLE key,
                                                   LPCWSTR property) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  DWORD size;
  if (FAILED(NCryptGetProperty(key, property, nullptr, 0, &size, 0))) {
    return std::nullopt;
  }

  std::vector<uint8_t> ret(size);
  if (FAILED(
          NCryptGetProperty(key, property, ret.data(), ret.size(), &size, 0))) {
    return std::nullopt;
  }
  CHECK_EQ(ret.size(), size);

  return ret;
}

// GetKeyStringProperty returns the given NCrypt key property of `key` as a
// string, removing the trailing null character if present.
std::optional<std::wstring> GetKeyStringProperty(NCRYPT_KEY_HANDLE key,
                                                 LPCWSTR property) {
  return GetKeyProperty(key, property)
      .transform([](base::span<const uint8_t> bytes) {
        auto chars = base::subtle::reinterpret_span<const wchar_t>(bytes);
        std::wstring_view str = {chars.data(), chars.size()};
        if (str.ends_with(L'\0')) {
          str.remove_suffix(1);
        }
        return std::wstring(str);
      });
}

// Returns true if the key has the NCRYPT_PCP_IDENTITY_KEY flag set in its
// usage policy. This flag indicates that the key is an Attestation Identity
// Key (AIK) restricted by the TPM, meaning it cannot be used to sign arbitrary
// data.
bool IsIdentityKey(NCRYPT_KEY_HANDLE key) {
  DWORD usage_policy = 0;
  DWORD cb_usage_policy = 0;
  SECURITY_STATUS status =
      NCryptGetProperty(key, NCRYPT_PCP_KEY_USAGE_POLICY_PROPERTY,
                        reinterpret_cast<PBYTE>(&usage_policy),
                        sizeof(usage_policy), &cb_usage_policy, 0);
  return SUCCEEDED(status) && ((usage_policy & NCRYPT_PCP_IDENTITY_KEY) != 0);
}

// Sets the NCRYPT_PCP_IDENTITY_KEY flag in the key's usage policy.
// This marks the key as an Attestation Identity Key (AIK). This property
// is specific to the Platform Crypto Provider (TPM) and restricts the key
// from being used to sign arbitrary data.
bool SetIdentityKeyPolicy(NCRYPT_KEY_HANDLE key) {
  DWORD usage_policy = NCRYPT_PCP_IDENTITY_KEY;
  SECURITY_STATUS status = NCryptSetProperty(
      key, NCRYPT_PCP_KEY_USAGE_POLICY_PROPERTY,
      reinterpret_cast<PBYTE>(&usage_policy), sizeof(usage_policy), 0);
  return SUCCEEDED(status);
}

// ExportKey returns |key| exported in the given format or nullopt on error.
base::expected<std::vector<uint8_t>, SECURITY_STATUS> ExportKey(
    NCRYPT_KEY_HANDLE key,
    LPCWSTR format) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  DWORD output_size;
  SECURITY_STATUS status =
      NCryptExportKey(key, 0, format, nullptr, nullptr, 0, &output_size, 0);
  if (FAILED(status)) {
    return base::unexpected(status);
  }

  std::vector<uint8_t> output(output_size);
  status = NCryptExportKey(key, 0, format, nullptr, output.data(),
                           output.size(), &output_size, 0);
  if (FAILED(status)) {
    return base::unexpected(status);
  }
  CHECK_EQ(output.size(), output_size);

  return output;
}

std::optional<std::vector<uint8_t>> GetP256ECDSASPKI(NCRYPT_KEY_HANDLE key) {
  const base::expected<std::vector<uint8_t>, SECURITY_STATUS> pub_key =
      ExportKey(key, BCRYPT_ECCPUBLIC_BLOB);
  if (!pub_key.has_value()) {
    return std::nullopt;
  }

  // The exported key is a |BCRYPT_ECCKEY_BLOB| followed by the bytes of the
  // public key itself.
  // https://docs.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_ecckey_blob
  BCRYPT_ECCKEY_BLOB header;
  if (pub_key->size() < sizeof(header)) {
    return std::nullopt;
  }
  UNSAFE_TODO(memcpy(&header, pub_key->data(), sizeof(header)));
  // |cbKey| is documented[1] as "the length, in bytes, of the key". It is
  // not. For ECDSA public keys it is the length of a field element.
  if ((header.dwMagic != BCRYPT_ECDSA_PUBLIC_P256_MAGIC &&
       header.dwMagic != BCRYPT_ECDSA_PUBLIC_GENERIC_MAGIC) ||
      header.cbKey != 256 / 8 ||
      pub_key->size() - sizeof(BCRYPT_ECCKEY_BLOB) != 64) {
    return std::nullopt;
  }

  // Sometimes NCrypt will return a generic dwMagic even when asked for a P-256
  // key. In that case, do extra validation to make sure that `key` is in fact
  // a P-256 key.
  if (header.dwMagic == BCRYPT_ECDSA_PUBLIC_GENERIC_MAGIC) {
    if (GetKeyStringProperty(key, NCRYPT_ECC_CURVE_NAME_PROPERTY) !=
        BCRYPT_ECC_CURVE_NISTP256) {
      return std::nullopt;
    }
  }

  uint8_t x962[1 + 32 + 32];
  UNSAFE_TODO(x962[0]) = POINT_CONVERSION_UNCOMPRESSED;
  UNSAFE_TODO(
      memcpy(&x962[1], pub_key->data() + sizeof(BCRYPT_ECCKEY_BLOB), 64));

  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_oct2point(p256.get(), point.get(), x962, sizeof(x962),
                          /*ctx=*/nullptr)) {
    return std::nullopt;
  }
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_set_public_key(ec_key.get(), point.get()));
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(EVP_PKEY_set1_EC_KEY(pkey.get(), ec_key.get()));

  bssl::ScopedCBB cbb;
  CHECK(CBB_init(cbb.get(), /*initial_capacity=*/128) &&
        EVP_marshal_public_key(cbb.get(), pkey.get()));
  return CBBToVector(cbb.get());
}

std::optional<std::vector<uint8_t>> GetRSASPKI(NCRYPT_KEY_HANDLE key) {
  const base::expected<std::vector<uint8_t>, SECURITY_STATUS> pub_key =
      ExportKey(key, BCRYPT_RSAPUBLIC_BLOB);
  if (!pub_key.has_value()) {
    return std::nullopt;
  }

  // The exported key is a |BCRYPT_RSAKEY_BLOB| followed by the bytes of the
  // key itself.
  // https://docs.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_rsakey_blob
  BCRYPT_RSAKEY_BLOB header;
  if (pub_key->size() < sizeof(header)) {
    return std::nullopt;
  }
  UNSAFE_TODO(memcpy(&header, pub_key->data(), sizeof(header)));
  if (header.Magic != static_cast<ULONG>(BCRYPT_RSAPUBLIC_MAGIC)) {
    return std::nullopt;
  }

  size_t bytes_needed;
  if (!base::CheckAdd(sizeof(BCRYPT_RSAKEY_BLOB),
                      base::CheckAdd(header.cbPublicExp, header.cbModulus))
           .AssignIfValid(&bytes_needed) ||
      pub_key->size() < bytes_needed) {
    return std::nullopt;
  }

  bssl::UniquePtr<BIGNUM> e(
      BN_bin2bn(UNSAFE_TODO(&pub_key->data()[sizeof(BCRYPT_RSAKEY_BLOB)]),
                header.cbPublicExp, nullptr));
  bssl::UniquePtr<BIGNUM> n(BN_bin2bn(
      UNSAFE_TODO(
          &pub_key->data()[sizeof(BCRYPT_RSAKEY_BLOB) + header.cbPublicExp]),
      header.cbModulus, nullptr));

  bssl::UniquePtr<RSA> rsa(RSA_new());
  CHECK(RSA_set0_key(rsa.get(), n.release(), e.release(), nullptr));
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(EVP_PKEY_set1_RSA(pkey.get(), rsa.get()));

  bssl::ScopedCBB cbb;
  CHECK(CBB_init(cbb.get(), /*initial_capacity=*/384) &&
        EVP_marshal_public_key(cbb.get(), pkey.get()));
  return CBBToVector(cbb.get());
}

base::expected<std::vector<uint8_t>, SECURITY_STATUS> SignECDSA(
    NCRYPT_KEY_HANDLE key,
    base::span<const uint8_t> data) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  std::array<uint8_t, hash::kSha256Size> digest = hash::Sha256(data);
  // The signature is written as a pair of big-endian field elements for P-256
  // ECDSA.
  std::vector<uint8_t> sig(64);
  DWORD sig_size;
  {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    SECURITY_STATUS status =
        NCryptSignHash(key, nullptr, digest.data(), digest.size(), sig.data(),
                       sig.size(), &sig_size, NCRYPT_SILENT_FLAG);
    if (FAILED(status)) {
      return base::unexpected(status);
    }
  }
  CHECK_EQ(sig.size(), sig_size);

  bssl::UniquePtr<BIGNUM> r(BN_bin2bn(sig.data(), 32, nullptr));
  bssl::UniquePtr<BIGNUM> s(
      BN_bin2bn(UNSAFE_TODO(sig.data() + 32), 32, nullptr));
  ECDSA_SIG sig_st;
  sig_st.r = r.get();
  sig_st.s = s.get();

  bssl::ScopedCBB cbb;
  CHECK(CBB_init(cbb.get(), /*initial_capacity=*/72) &&
        ECDSA_SIG_marshal(cbb.get(), &sig_st));
  return CBBToVector(cbb.get());
}

base::expected<std::vector<uint8_t>, SECURITY_STATUS> SignRSA(
    NCRYPT_KEY_HANDLE key,
    base::span<const uint8_t> data) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::WILL_BLOCK);

  std::array<uint8_t, hash::kSha256Size> digest = hash::Sha256(data);
  BCRYPT_PKCS1_PADDING_INFO padding_info = {0};
  padding_info.pszAlgId = NCRYPT_SHA256_ALGORITHM;

  DWORD sig_size;
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  SECURITY_STATUS status =
      NCryptSignHash(key, &padding_info, digest.data(), digest.size(), nullptr,
                     0, &sig_size, NCRYPT_SILENT_FLAG | BCRYPT_PAD_PKCS1);
  if (FAILED(status)) {
    return base::unexpected(status);
  }

  std::vector<uint8_t> sig(sig_size);
  status = NCryptSignHash(key, &padding_info, digest.data(), digest.size(),
                          sig.data(), sig.size(), &sig_size,
                          NCRYPT_SILENT_FLAG | BCRYPT_PAD_PKCS1);
  if (FAILED(status)) {
    return base::unexpected(status);
  }
  CHECK_EQ(sig.size(), sig_size);

  return sig;
}

ScopedNCryptKey LoadWrappedKey(base::span<const uint8_t> wrapped,
                               ProviderType provider_type) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  ScopedNCryptProvider provider;
  SECURITY_STATUS status =
      NCryptOpenStorageProvider(ScopedNCryptProvider::Receiver(provider).get(),
                                GetWindowsIdentifierForProvider(provider_type),
                                /*flags=*/0);
  if (FAILED(status)) {
    LogTPMOperationError(TPMOperation::kWrappedKeyCreation, status,
                         std::nullopt, /*open_storage_provider_error=*/true);
    return ScopedNCryptKey();
  }

  ScopedNCryptKey key;
  SECURITY_STATUS import_status = -1;
  if (provider_type == ProviderType::kSoftware) {
    // Software keys are labelled with a random identifier. Attempt to obtain a
    // handle from the identifier.
    std::u16string key_label = KeyIdToWindowsLabel(wrapped);
    import_status =
        NCryptOpenKey(provider.get(), ScopedNCryptKey::Receiver(key).get(),
                      base::as_wcstr(key_label),
                      /*dwLegacyKeySpec=*/0, /*dwFlags=*/0);
  } else {
    // TPM keys use an undocumented Windows feature to export a wrapped key.
    // Attempt to obtain a handle from the wrapped key.
    import_status = NCryptImportKey(
        provider.get(), /*hImportKey=*/NULL, BCRYPT_OPAQUE_KEY_BLOB,
        /*pParameterList=*/nullptr, ScopedNCryptKey::Receiver(key).get(),
        const_cast<PBYTE>(wrapped.data()), wrapped.size(),
        /*dwFlags=*/NCRYPT_SILENT_FLAG);
  }
  if (FAILED(import_status)) {
    LogTPMOperationError(TPMOperation::kWrappedKeyCreation, import_status,
                         std::nullopt);
    return ScopedNCryptKey();
  }
  return key;
}

// ECDSASigningKey wraps a P-256 ECDSA key stored in the given provider.
class ECDSASigningKey : public WinKeyImpl<UnexportableSigningKey> {
 public:
  ECDSASigningKey(ProviderType provider_type, KeyDetails details)
      : WinKeyImpl(provider_type, std::move(details)) {}

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    return base::OptionalFromExpected(
        SignECDSA(key_.get(), data)
            .transform_error([&](SECURITY_STATUS status) {
              LogTPMOperationError(TPMOperation::kMessageSigning, status,
                                   Algorithm());
              return status;
            }));
  }

  bool SupportsTls13() override { return true; }
};

// RSASigningKey wraps a RSA key stored in the given provider.
class RSASigningKey : public WinKeyImpl<UnexportableSigningKey> {
 public:
  RSASigningKey(ProviderType provider_type, KeyDetails details)
      : WinKeyImpl(provider_type, std::move(details)) {}

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    return base::OptionalFromExpected(
        SignRSA(key_.get(), data).transform_error([&](SECURITY_STATUS status) {
          LogTPMOperationError(TPMOperation::kMessageSigning, status,
                               Algorithm());
          return status;
        }));
  }

  bool SupportsTls13() override {
    if (!is_compatible_with_tls13.has_value()) {
      is_compatible_with_tls13 = CanSignPssWithExpectedSaltLength();
    }

    return is_compatible_with_tls13.value();
  }

 private:
  bool CanSignPssWithExpectedSaltLength() {
    // TLS 1.3 requires support of RSA-PSS algorithm with Salt Length == Hash
    // Length (32 bytes for SHA-256).
    BCRYPT_PSS_PADDING_INFO padding_info = {0};
    padding_info.pszAlgId = BCRYPT_SHA256_ALGORITHM;
    padding_info.cbSalt = 32;

    constexpr auto dummy_data = std::to_array<uint8_t>({
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    });

    auto dummy_hash = hash::Sha256(dummy_data);
    DWORD cb_signature = 0;

    if (FAILED(NCryptSignHash(key_.get(), &padding_info, dummy_hash.data(),
                              dummy_hash.size(), nullptr, 0, &cb_signature,
                              NCRYPT_SILENT_FLAG | NCRYPT_PAD_PSS_FLAG))) {
      return false;
    }

    std::vector<uint8_t> signature(cb_signature);
    if (FAILED(NCryptSignHash(key_.get(), &padding_info, dummy_hash.data(),
                              dummy_hash.size(), signature.data(),
                              signature.size(), &cb_signature,
                              NCRYPT_SILENT_FLAG | NCRYPT_PAD_PSS_FLAG))) {
      return false;
    }

    auto public_key = keypair::PublicKey::FromSubjectPublicKeyInfo(spki_);
    if (!public_key) {
      return false;
    }

    return sign::Verify(sign::SignatureKind::RSA_PSS_SHA256, public_key.value(),
                        dummy_data, signature);
  }

  std::optional<bool> is_compatible_with_tls13;
};

// AttestationKeyWin wraps an AIK stored in the given provider.
class AttestationKeyWin : public WinKeyImpl<UnexportableAttestationKey> {
 public:
  AttestationKeyWin(ProviderType provider_type, KeyDetails details)
      : WinKeyImpl(provider_type, std::move(details)) {}

  std::optional<AttestationStatement> CertifySlowly(
      const UnexportableSigningKey& signing_key,
      base::span<const uint8_t> challenge) override {
    // TPM certification execution not yet implemented.
    return std::nullopt;
  }
};

// UnexportableKeyProviderWin uses NCrypt and the Platform Crypto
// Provider to expose TPM-backed keys on Windows.
class UnexportableKeyProviderWin : public UnexportableKeyProvider {
 public:
  explicit UnexportableKeyProviderWin(ProviderType provider_type)
      : provider_type_(provider_type) {}
  ~UnexportableKeyProviderWin() override = default;

  std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    ScopedNCryptProvider provider;
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      SECURITY_STATUS status = NCryptOpenStorageProvider(
          ScopedNCryptProvider::Receiver(provider).get(),
          GetWindowsIdentifierForProvider(provider_type_), /*flags=*/0);
      if (FAILED(status)) {
        LogTPMOperationError(TPMOperation::kSelectAlgorithm, status,
                             std::nullopt,
                             /*open_storage_provider_error=*/true);
        return std::nullopt;
      }
    }

    return GetBestSupported(provider.get(), acceptable_algorithms);
  }

  std::optional<KeyDetails> GenerateKeyImpl(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      KeyUsage usage) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    ScopedNCryptProvider provider;
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      SECURITY_STATUS status = NCryptOpenStorageProvider(
          ScopedNCryptProvider::Receiver(provider).get(),
          GetWindowsIdentifierForProvider(provider_type_), /*flags=*/0);
      if (FAILED(status)) {
        LogTPMOperationError(TPMOperation::kNewKeyCreation, status,
                             std::nullopt,
                             /*open_storage_provider_error=*/true);
        return std::nullopt;
      }
    }

    ASSIGN_OR_RETURN(SignatureVerifier::SignatureAlgorithm algo,
                     GetBestSupported(provider.get(), acceptable_algorithms));

    std::vector<uint8_t> key_id;
    ScopedNCryptKey key;
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

      SECURITY_STATUS creation_status;
      if (provider_type_ == ProviderType::kSoftware) {
        // Windows support for wrapped keys is undocumented, and doesn't seem to
        // work for the software backend. The API wants Chrome to provide a
        // label for the key, so we assign one randomly.
        key_id = crypto::RandBytesAsVector(16);
        std::u16string key_label = KeyIdToWindowsLabel(key_id);
        creation_status = NCryptCreatePersistedKey(
            provider.get(), ScopedNCryptKey::Receiver(key).get(),
            BCryptAlgorithmFor(algo).value(), base::as_wcstr(key_label),
            /*dwLegacyKeySpec=*/0, /*dwFlags=*/0);
      } else {
        // An empty key name stops the key being persisted to disk.
        // TODO(crbug.com/398125799): assign labels to these keys instead.
        creation_status = NCryptCreatePersistedKey(
            provider.get(), ScopedNCryptKey::Receiver(key).get(),
            BCryptAlgorithmFor(algo).value(),
            /*pszKeyName=*/nullptr,
            /*dwLegacyKeySpec=*/0, /*dwFlags=*/0);
      }
      if (FAILED(creation_status)) {
        LogTPMOperationError(TPMOperation::kNewKeyCreation, creation_status,
                             algo);
        return std::nullopt;
      }

      if (usage == KeyUsage::kAttestation && !SetIdentityKeyPolicy(key.get())) {
        return std::nullopt;
      }

      if (FAILED(NCryptFinalizeKey(key.get(), NCRYPT_SILENT_FLAG))) {
        return std::nullopt;
      }
    }
    if (provider_type_ == ProviderType::kTPM) {
      ASSIGN_OR_RETURN(key_id, ExportKey(key.get(), BCRYPT_OPAQUE_KEY_BLOB),
                       [&](SECURITY_STATUS status) {
                         LogTPMOperationError(TPMOperation::kWrappedKeyExport,
                                              status, algo);
                         return std::nullopt;
                       });
    }

    ASSIGN_OR_RETURN(
        std::vector<uint8_t> spki,
        [&]() -> std::optional<std::vector<uint8_t>> {
          switch (algo) {
            case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
              return GetP256ECDSASPKI(key.get());
            case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
              return GetRSASPKI(key.get());
            default:
              return std::nullopt;
          }
        }());

    return KeyDetails{std::move(key), std::move(key_id), std::move(spki), algo};
  }

  std::optional<KeyDetails> FromWrappedKeyImpl(
      base::span<const uint8_t> wrapped,
      KeyUsage usage) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    ScopedNCryptKey key = LoadWrappedKey(wrapped, provider_type_);
    if (!key.is_valid()) {
      return std::nullopt;
    }

    if ((usage == KeyUsage::kAttestation) != IsIdentityKey(key.get())) {
      return std::nullopt;
    }

    // The documentation suggests that |NCRYPT_ALGORITHM_PROPERTY| should return
    // the original algorithm, i.e. |BCRYPT_ECDSA_P256_ALGORITHM| for ECDSA. But
    // it actually returns just "ECDSA" for keys backed by the TPM.
    ASSIGN_OR_RETURN(
        std::wstring algorithm,
        GetKeyStringProperty(key.get(), NCRYPT_ALGORITHM_PROPERTY));

    if (algorithm == BCRYPT_ECDSA_P256_ALGORITHM ||
        algorithm == BCRYPT_ECDSA_ALGORITHM) {
      ASSIGN_OR_RETURN(std::vector<uint8_t> spki, GetP256ECDSASPKI(key.get()));
      return KeyDetails{std::move(key), base::ToVector(wrapped),
                        std::move(spki),
                        SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256};
    }

    if (algorithm == BCRYPT_RSA_ALGORITHM) {
      ASSIGN_OR_RETURN(std::vector<uint8_t> spki, GetRSASPKI(key.get()));
      return KeyDetails{
          std::move(key), base::ToVector(wrapped), std::move(spki),
          SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256};
    }

    return std::nullopt;
  }

  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    ASSIGN_OR_RETURN(KeyDetails key,
                     GenerateKeyImpl(acceptable_algorithms, KeyUsage::kSigning),
                     [] { return nullptr; });

    switch (key.algo) {
      case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        return std::make_unique<ECDSASigningKey>(provider_type_,
                                                 std::move(key));
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        return std::make_unique<RSASigningKey>(provider_type_, std::move(key));
      default:
        return nullptr;
    }
  }

  std::unique_ptr<UnexportableAttestationKey> GenerateAttestationKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    ASSIGN_OR_RETURN(
        KeyDetails key,
        GenerateKeyImpl(acceptable_algorithms, KeyUsage::kAttestation),
        [] { return nullptr; });

    return std::make_unique<AttestationKeyWin>(provider_type_, std::move(key));
  }

  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped) override {
    ASSIGN_OR_RETURN(KeyDetails key,
                     FromWrappedKeyImpl(wrapped, KeyUsage::kSigning),
                     [] { return nullptr; });

    switch (key.algo) {
      case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        return std::make_unique<ECDSASigningKey>(provider_type_,
                                                 std::move(key));
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        return std::make_unique<RSASigningKey>(provider_type_, std::move(key));
      default:
        return nullptr;
    }
  }

  std::unique_ptr<UnexportableAttestationKey> FromWrappedAttestationKeySlowly(
      base::span<const uint8_t> wrapped) override {
    ASSIGN_OR_RETURN(KeyDetails key,
                     FromWrappedKeyImpl(wrapped, KeyUsage::kAttestation),
                     [] { return nullptr; });

    return std::make_unique<AttestationKeyWin>(provider_type_, std::move(key));
  }

  StatefulUnexportableKeyProvider* AsStatefulUnexportableKeyProvider()
      override {
    // Unexportable keys are stateless on Windows.
    return nullptr;
  }

 private:
  ProviderType provider_type_;
};

// ECDSASoftwareKey wraps a Credential Guard stored P-256 ECDSA key.
class ECDSASoftwareKey : public VirtualUnexportableSigningKey {
 public:
  ECDSASoftwareKey(ScopedNCryptKey key,
                   std::string name,
                   std::vector<uint8_t> spki)
      : key_(std::move(key)), name_(std::move(name)), spki_(std::move(spki)) {}

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    return SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return spki_;
  }

  std::string GetKeyName() const override { return name_; }

  std::optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> data) override {
    if (!key_.is_valid()) {
      return std::nullopt;
    }

    return base::OptionalFromExpected(SignECDSA(key_.get(), data));
  }

  void DeleteKey() override {
    if (!key_.is_valid()) {
      return;
    }

    // If key deletion succeeds, NCryptDeleteKey frees the key. To avoid double
    // free, we need to release the key from the ScopedNCryptKey RAII object.
    // Key deletion can fail in circumstances which are not under the
    // application's control. For these cases, ScopedNCrypt key should free the
    // key.
    if (NCryptDeleteKey(key_.get(), NCRYPT_SILENT_FLAG) == ERROR_SUCCESS) {
      static_cast<void>(key_.release());
    }
  }

 private:
  ScopedNCryptKey key_;
  const std::string name_;
  const std::vector<uint8_t> spki_;
};

// RSASoftwareKey wraps a Credential Guard stored RSA key.
class RSASoftwareKey : public VirtualUnexportableSigningKey {
 public:
  RSASoftwareKey(ScopedNCryptKey key,
                 std::string name,
                 std::vector<uint8_t> spki)
      : key_(std::move(key)), name_(std::move(name)), spki_(std::move(spki)) {}

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    return SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return spki_;
  }

  std::string GetKeyName() const override { return name_; }

  std::optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> data) override {
    if (!key_.is_valid()) {
      return std::nullopt;
    }

    return base::OptionalFromExpected(SignRSA(key_.get(), data));
  }

  void DeleteKey() override {
    if (!key_.is_valid()) {
      return;
    }

    // If key deletion succeeds, NCryptDeleteKey frees the key. To avoid double
    // free, we need to release the key from the ScopedNCryptKey RAII object.
    // Key deletion can fail in circumstances which are not under the
    // application's control. For these cases, ScopedNCrypt key should free the
    // key.
    if (NCryptDeleteKey(key_.get(), NCRYPT_SILENT_FLAG) == ERROR_SUCCESS) {
      static_cast<void>(key_.release());
    }
  }

 private:
  ScopedNCryptKey key_;
  std::string name_;
  const std::vector<uint8_t> spki_;
};

// UnexportableKeyProviderWin uses NCrypt and the Platform Crypto
// Provider to expose Credential Guard backed keys on Windows.
class VirtualUnexportableKeyProviderWin
    : public VirtualUnexportableKeyProvider {
 public:
  ~VirtualUnexportableKeyProviderWin() override = default;

  std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    ScopedNCryptProvider provider;
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      SECURITY_STATUS status = NCryptOpenStorageProvider(
          ScopedNCryptProvider::Receiver(provider).get(),
          MS_KEY_STORAGE_PROVIDER, /*dwFlags=*/0);
      if (FAILED(status)) {
        base::UmaHistogramSparse(kMetricVirtualOpenStorageError, status);
        return std::nullopt;
      }
    }

    return GetBestSupported(provider.get(), acceptable_algorithms);
  }

  std::unique_ptr<VirtualUnexportableSigningKey> GenerateSigningKey(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      std::string name) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    ScopedNCryptProvider provider;
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      SECURITY_STATUS status = NCryptOpenStorageProvider(
          ScopedNCryptProvider::Receiver(provider).get(),
          MS_KEY_STORAGE_PROVIDER, /*dwFlags=*/0);
      if (FAILED(status)) {
        base::UmaHistogramSparse(kMetricVirtualOpenStorageError, status);
        return nullptr;
      }
    }

    std::optional<SignatureVerifier::SignatureAlgorithm> algo =
        GetBestSupported(provider.get(), acceptable_algorithms);
    if (!algo) {
      return nullptr;
    }

    ScopedNCryptKey key;
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      // An empty key name stops the key being persisted to disk.
      SECURITY_STATUS status = NCryptCreatePersistedKey(
          provider.get(), ScopedNCryptKey::Receiver(key).get(),
          BCryptAlgorithmFor(*algo).value(), base::SysUTF8ToWide(name).c_str(),
          /*dwLegacyKeySpec=*/0,
          /*dwFlags=*/NCRYPT_USE_VIRTUAL_ISOLATION_FLAG);
      if (FAILED(status)) {
        base::UmaHistogramSparse(kMetricVirtualCreateKeyError, status);
        return nullptr;
      }

      status = NCryptFinalizeKey(
          key.get(), NCRYPT_PROTECT_TO_LOCAL_SYSTEM | NCRYPT_SILENT_FLAG);
      if (FAILED(status)) {
        base::UmaHistogramSparse(kMetricVirtualFinalizeKeyError, status);
        return nullptr;
      }
    }

    std::optional<std::vector<uint8_t>> spki;
    switch (*algo) {
      case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        spki = GetP256ECDSASPKI(key.get());
        if (!spki) {
          return nullptr;
        }
        return std::make_unique<ECDSASoftwareKey>(std::move(key), name,
                                                  std::move(spki.value()));
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        spki = GetRSASPKI(key.get());
        if (!spki) {
          return nullptr;
        }
        return std::make_unique<RSASoftwareKey>(std::move(key), name,
                                                std::move(spki.value()));
      default:
        return nullptr;
    }
  }

  std::unique_ptr<VirtualUnexportableSigningKey> FromKeyName(
      std::string name) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    ScopedNCryptProvider provider;
    ScopedNCryptKey key;
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      SECURITY_STATUS status = NCryptOpenStorageProvider(
          ScopedNCryptProvider::Receiver(provider).get(),
          MS_KEY_STORAGE_PROVIDER, /*dwFlags=*/0);
      if (FAILED(status)) {
        base::UmaHistogramSparse(kMetricVirtualOpenStorageError, status);
        return nullptr;
      }

      status = NCryptOpenKey(
          provider.get(), ScopedNCryptKey::Receiver(key).get(),
          base::SysUTF8ToWide(name).c_str(), /*dwLegacyKeySpec=*/0,
          /*dwFlags*/ 0);
      if (FAILED(status)) {
        base::UmaHistogramSparse(kMetricVirtualOpenKeyError, status);
        return nullptr;
      }
    }

    ASSIGN_OR_RETURN(std::wstring algorithm,
                     GetKeyStringProperty(key.get(), NCRYPT_ALGORITHM_PROPERTY),
                     [] { return nullptr; });

    // This is the expected behavior, but note it is different from TPM backed
    // keys.
    if (algorithm == BCRYPT_ECDSA_P256_ALGORITHM) {
      ASSIGN_OR_RETURN(std::vector<uint8_t> spki, GetP256ECDSASPKI(key.get()),
                       [] { return nullptr; });
      return std::make_unique<ECDSASoftwareKey>(std::move(key), name,
                                                std::move(spki));
    } else if (algorithm == BCRYPT_RSA_ALGORITHM) {
      ASSIGN_OR_RETURN(std::vector<uint8_t> spki, GetRSASPKI(key.get()),
                       [] { return nullptr; });
      return std::make_unique<RSASoftwareKey>(std::move(key), name,
                                              std::move(spki));
    }

    return nullptr;
  }
};

}  // namespace

ScopedNCryptKey DuplicatePlatformKeyHandle(const UnexportableKey& key) {
  return LoadWrappedKey(key.GetWrappedKey(), key.IsHardwareBacked()
                                                 ? ProviderType::kTPM
                                                 : ProviderType::kSoftware);
}

std::unique_ptr<UnexportableKeyProvider> GetUnexportableKeyProviderWin() {
  return std::make_unique<UnexportableKeyProviderWin>(ProviderType::kTPM);
}

std::unique_ptr<UnexportableKeyProvider>
GetMicrosoftSoftwareUnexportableKeyProviderWin() {
  return std::make_unique<UnexportableKeyProviderWin>(ProviderType::kSoftware);
}

std::unique_ptr<VirtualUnexportableKeyProvider>
GetVirtualUnexportableKeyProviderWin() {
  return std::make_unique<VirtualUnexportableKeyProviderWin>();
}

}  // namespace crypto
