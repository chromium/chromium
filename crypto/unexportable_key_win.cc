// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "crypto/unexportable_key_win.h"

#include <string>
#include <tuple>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
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
#include "base/types/optional_util.h"
#include "crypto/features.h"
#include "crypto/hash.h"
#include "crypto/random.h"
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
    std::optional<SignatureVerifier::SignatureAlgorithm> selected_algorithm) {
  static constexpr char kCreateKeyErrorStatusHistogramFormat[] =
      "Crypto.TPMOperation.Win.%s%s.Error";
  // Only `kWrappedKeyCreation` could and should be recorded without
  // `selected_algorithm`.
  CHECK_EQ(!selected_algorithm.has_value(),
           operation == TPMOperation::kWrappedKeyCreation);
  std::string algorithm_string =
      selected_algorithm ? AlgorithmToString(*selected_algorithm) : "";
  base::UmaHistogramSparse(
      base::StringPrintf(kCreateKeyErrorStatusHistogramFormat,
                         OperationToString(operation).c_str(),
                         algorithm_string.c_str()),
      status);
}

std::vector<uint8_t> CBBToVector(const CBB* cbb) {
  return std::vector<uint8_t>(CBB_data(cbb), CBB_data(cbb) + CBB_len(cbb));
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
    if (!FAILED(NCryptIsAlgSupported(provider, *bcrypto_algo_name,
                                     /*flags=*/0))) {
      return algo;
    }
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
  memcpy(&header, pub_key->data(), sizeof(header));
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
    const std::optional<std::vector<uint8_t>> curve_name =
        GetKeyProperty(key, NCRYPT_ECC_CURVE_NAME_PROPERTY);
    if (!curve_name) {
      return std::nullopt;
    }

    if (curve_name->size() != sizeof(BCRYPT_ECC_CURVE_NISTP256) ||
        memcmp(curve_name->data(), BCRYPT_ECC_CURVE_NISTP256,
               sizeof(BCRYPT_ECC_CURVE_NISTP256)) != 0) {
      return std::nullopt;
    }
  }

  uint8_t x962[1 + 32 + 32];
  x962[0] = POINT_CONVERSION_UNCOMPRESSED;
  memcpy(&x962[1], pub_key->data() + sizeof(BCRYPT_ECCKEY_BLOB), 64);

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
  memcpy(&header, pub_key->data(), sizeof(header));
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
      BN_bin2bn(&pub_key->data()[sizeof(BCRYPT_RSAKEY_BLOB)],
                header.cbPublicExp, nullptr));
  bssl::UniquePtr<BIGNUM> n(BN_bin2bn(
      &pub_key->data()[sizeof(BCRYPT_RSAKEY_BLOB) + header.cbPublicExp],
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
  bssl::UniquePtr<BIGNUM> s(BN_bin2bn(sig.data() + 32, 32, nullptr));
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
  if (FAILED(NCryptOpenStorageProvider(
          ScopedNCryptProvider::Receiver(provider).get(),
          GetWindowsIdentifierForProvider(provider_type),
          /*flags=*/0))) {
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

// ECDSAKey wraps a P-256 ECDSA key stored in the given provider.
class ECDSAKey : public UnexportableSigningKey {
 public:
  ECDSAKey(ProviderType provider_type,
           ScopedNCryptKey key,
           std::vector<uint8_t> key_id,
           std::vector<uint8_t> spki)
      : provider_type_(provider_type),
        key_(std::move(key)),
        key_id_(std::move(key_id)),
        spki_(std::move(spki)) {}

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    return SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return spki_;
  }

  std::vector<uint8_t> GetWrappedKey() const override { return key_id_; }

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    base::expected<std::vector<uint8_t>, SECURITY_STATUS> signature =
        SignECDSA(key_.get(), data);
    if (!signature.has_value()) {
      LogTPMOperationError(TPMOperation::kMessageSigning, signature.error(),
                           Algorithm());
    }

    return base::OptionalFromExpected(signature);
  }

  bool IsHardwareBacked() const override {
    return base::FeatureList::IsEnabled(features::kIsHardwareBackedFixEnabled)
               ? provider_type_ == ProviderType::kTPM
               : true;
  }

 private:
  const ProviderType provider_type_;
  ScopedNCryptKey key_;
  const std::vector<uint8_t> key_id_;
  const std::vector<uint8_t> spki_;
};

// RSAKey wraps a RSA key stored in the given provider.
class RSAKey : public UnexportableSigningKey {
 public:
  RSAKey(ProviderType provider_type,
         ScopedNCryptKey key,
         std::vector<uint8_t> wrapped,
         std::vector<uint8_t> spki)
      : provider_type_(provider_type),
        key_(std::move(key)),
        wrapped_(std::move(wrapped)),
        spki_(std::move(spki)) {}

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    return SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return spki_;
  }

  std::vector<uint8_t> GetWrappedKey() const override { return wrapped_; }

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    base::expected<std::vector<uint8_t>, SECURITY_STATUS> signature =
        SignRSA(key_.get(), data);
    if (!signature.has_value()) {
      LogTPMOperationError(TPMOperation::kMessageSigning, signature.error(),
                           Algorithm());
    }

    return base::OptionalFromExpected(signature);
  }

  bool IsHardwareBacked() const override {
    return base::FeatureList::IsEnabled(features::kIsHardwareBackedFixEnabled)
               ? provider_type_ == ProviderType::kTPM
               : true;
  }

 private:
  const ProviderType provider_type_;
  ScopedNCryptKey key_;
  const std::vector<uint8_t> wrapped_;
  const std::vector<uint8_t> spki_;
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
      if (FAILED(NCryptOpenStorageProvider(
              ScopedNCryptProvider::Receiver(provider).get(),
              GetWindowsIdentifierForProvider(provider_type_), /*flags=*/0))) {
        return std::nullopt;
      }
    }

    return GetBestSupported(provider.get(), acceptable_algorithms);
  }

  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    ScopedNCryptProvider provider;
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      if (FAILED(NCryptOpenStorageProvider(
              ScopedNCryptProvider::Receiver(provider).get(),
              GetWindowsIdentifierForProvider(provider_type_), /*flags=*/0))) {
        return nullptr;
      }
    }

    std::optional<SignatureVerifier::SignatureAlgorithm> algo =
        GetBestSupported(provider.get(), acceptable_algorithms);
    if (!algo) {
      return nullptr;
    }

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
            BCryptAlgorithmFor(*algo).value(), base::as_wcstr(key_label),
            /*dwLegacyKeySpec=*/0, /*dwFlags=*/0);
      } else {
        // An empty key name stops the key being persisted to disk.
        // TODO(crbug.com/398125799): assign labels to these keys instead.
        creation_status = NCryptCreatePersistedKey(
            provider.get(), ScopedNCryptKey::Receiver(key).get(),
            BCryptAlgorithmFor(*algo).value(),
            /*pszKeyName=*/nullptr,
            /*dwLegacyKeySpec=*/0, /*dwFlags=*/0);
      }
      if (FAILED(creation_status)) {
        LogTPMOperationError(TPMOperation::kNewKeyCreation, creation_status,
                             algo);
        return nullptr;
      }

      if (FAILED(NCryptFinalizeKey(key.get(), NCRYPT_SILENT_FLAG))) {
        return nullptr;
      }
    }

    if (provider_type_ == ProviderType::kTPM) {
      base::expected<std::vector<uint8_t>, SECURITY_STATUS> wrapped_key =
          ExportKey(key.get(), BCRYPT_OPAQUE_KEY_BLOB);
      if (!wrapped_key.has_value()) {
        LogTPMOperationError(TPMOperation::kWrappedKeyExport,
                             wrapped_key.error(), algo);
        return nullptr;
      }
      key_id = std::move(wrapped_key.value());
    }

    std::optional<std::vector<uint8_t>> spki;
    switch (*algo) {
      case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        spki = GetP256ECDSASPKI(key.get());
        if (!spki) {
          return nullptr;
        }
        return std::make_unique<ECDSAKey>(provider_type_, std::move(key),
                                          std::move(key_id),
                                          std::move(spki.value()));
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        spki = GetRSASPKI(key.get());
        if (!spki) {
          return nullptr;
        }
        return std::make_unique<RSAKey>(provider_type_, std::move(key),
                                        std::move(key_id),
                                        std::move(spki.value()));
      default:
        return nullptr;
    }
  }

  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    ScopedNCryptKey key = LoadWrappedKey(wrapped, provider_type_);
    if (!key.is_valid()) {
      return nullptr;
    }

    const std::optional<std::vector<uint8_t>> algo_bytes =
        GetKeyProperty(key.get(), NCRYPT_ALGORITHM_PROPERTY);
    if (!algo_bytes) {
      return nullptr;
    }

    // The documentation suggests that |NCRYPT_ALGORITHM_PROPERTY| should return
    // the original algorithm, i.e. |BCRYPT_ECDSA_P256_ALGORITHM| for ECDSA. But
    // it actually returns just "ECDSA" for keys backed by the TPM.
    // Note that these intentionally include the NUL terminator, since they're
    // comparing against a c-style string that happens to be represented as an
    // std::vector.
    static constexpr wchar_t kECDSA[] = L"ECDSA";
    static const base::span<const uint8_t> kECDSA_TPM =
        base::as_byte_span(kECDSA);
    static const base::span<const uint8_t> kECDSA_Software =
        base::as_byte_span(BCRYPT_ECDSA_P256_ALGORITHM);
    static const base::span<const uint8_t> kRSA =
        base::as_byte_span(BCRYPT_RSA_ALGORITHM);

    std::optional<std::vector<uint8_t>> spki;
    if (algo_bytes == kECDSA_Software || algo_bytes == kECDSA_TPM) {
      spki = GetP256ECDSASPKI(key.get());
      if (!spki) {
        return nullptr;
      }
      return std::make_unique<ECDSAKey>(
          provider_type_, std::move(key),
          std::vector<uint8_t>(wrapped.begin(), wrapped.end()),
          std::move(spki.value()));
    } else if (algo_bytes == kRSA) {
      spki = GetRSASPKI(key.get());
      if (!spki) {
        return nullptr;
      }
      return std::make_unique<RSAKey>(
          provider_type_, std::move(key),
          std::vector<uint8_t>(wrapped.begin(), wrapped.end()),
          std::move(spki.value()));
    }

    return nullptr;
  }

  bool DeleteSigningKeySlowly(base::span<const uint8_t> wrapped) override {
    // Unexportable keys are stateless on Windows.
    return true;
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

    const std::optional<std::vector<uint8_t>> algo_bytes =
        GetKeyProperty(key.get(), NCRYPT_ALGORITHM_PROPERTY);

    // This is the expected behavior, but note it is different from TPM backed
    // keys.
    // Note that these intentionally include the NUL terminator, since they're
    // comparing against a c-style string that happens to be represented as an
    // std::vector.
    static const base::span<const uint8_t> kECDSA_Software =
        base::as_byte_span(BCRYPT_ECDSA_P256_ALGORITHM);
    static const base::span<const uint8_t> kRSA =
        base::as_byte_span(BCRYPT_RSA_ALGORITHM);

    std::optional<std::vector<uint8_t>> spki;
    if (algo_bytes == kECDSA_Software) {
      spki = GetP256ECDSASPKI(key.get());
      if (!spki) {
        return nullptr;
      }
      return std::make_unique<ECDSASoftwareKey>(std::move(key), name,
                                                std::move(spki.value()));
    } else if (algo_bytes == kRSA) {
      spki = GetRSASPKI(key.get());
      if (!spki) {
        return nullptr;
      }
      return std::make_unique<RSASoftwareKey>(std::move(key), name,
                                              std::move(spki.value()));
    }

    return nullptr;
  }
};

}  // namespace

ScopedNCryptKey DuplicatePlatformKeyHandle(const UnexportableSigningKey& key) {
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
