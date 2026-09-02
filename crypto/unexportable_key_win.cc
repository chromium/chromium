// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key_win.h"

#include <ncrypt.h>
#include <tbs.h>

#include <array>
#include <concepts>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_rust.h"
#include "base/containers/span_writer.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
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
#include "base/win/delayload_helpers.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/random.h"
#include "crypto/sign.h"
#include "crypto/tpm_parser.h"
#include "crypto/unexportable_key.h"
#include "crypto/unexportable_key_metrics.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/boringssl/src/include/openssl/ec.h"

namespace crypto {

namespace {

// Persistent Storage Root Key (SRK) handles used as parent keys for TPM 2.0
// keys on Windows.
//
// In the TCG TPM 2.0 handle registry, 0x81000001 is reserved for the primary
// RSA Storage Root Key (SRK), while 0x81000002 is recommended for ECC. However,
// Windows Platform Crypto Provider (PCP) systems typically use 0x81000002 for
// an RSA signing key and provision the persistent ECC Storage Root Key at
// handle 0x81000009 (identified empirically via TPM2_GetCapability for
// TPM_CAP_HANDLES in the 0x81000000 range with TPM_ALG_ECC and
// restricted|decrypt attributes).
enum class WindowsSrkHandle : uint32_t {
  kRsa = 0x81000001,
  kEcc = 0x81000009,
};

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

  NCRYPT_KEY_HANDLE GetNCryptKeyHandle() const override { return key_.get(); }

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

template <typename T>
using SecurityStatusOr = base::expected<T, SECURITY_STATUS>;

template <typename T>
SecurityStatusOr<T> GetNCryptProperty(NCRYPT_HANDLE handle, LPCWSTR property) {
  T value{};
  DWORD cb_value = 0;
  SECURITY_STATUS status =
      NCryptGetProperty(handle, property, reinterpret_cast<PBYTE>(&value),
                        sizeof(value), &cb_value, 0);
  if (FAILED(status)) {
    return base::unexpected(status);
  }
  CHECK_EQ(cb_value, sizeof(value));
  return base::ok(value);
}

template <typename T>
SecurityStatusOr<void> SetNCryptProperty(NCRYPT_HANDLE handle,
                                         LPCWSTR property,
                                         T value) {
  SECURITY_STATUS status = NCryptSetProperty(
      handle, property, reinterpret_cast<PBYTE>(&value), sizeof(value), 0);
  return SUCCEEDED(status) ? SecurityStatusOr<void>()
                           : base::unexpected(status);
}

// Logs `status` and `selected_algorithm` to an error histogram capturing that
// `operation` failed for a TPM-backed key.
void LogTPMOperationError(
    TPMOperation operation,
    HRESULT status,
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
             (operation == TPMOperation::kWrappedKeyCreation ||
              operation == TPMOperation::kWrappedAttestationKeyCreation));
  }

  std::string algorithm_string =
      selected_algorithm ? AlgorithmToString(*selected_algorithm) : "";
  base::UmaHistogramSparse(
      base::StringPrintf(kTPMOperationErrorHistogramFormat,
                         OperationToString(operation).c_str(),
                         algorithm_string.c_str()),
      status);
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

// GetSrkHandleFor returns the persistent Storage Root Key (SRK) handle used as
// the parent key when creating TPM 2.0 keys for the given algorithm.
WindowsSrkHandle GetSrkHandleFor(SignatureVerifier::SignatureAlgorithm algo) {
  switch (algo) {
    case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
    case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
    case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
      return WindowsSrkHandle::kRsa;
    case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      return WindowsSrkHandle::kEcc;
  }
  NOTREACHED();
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
  auto usage_policy =
      GetNCryptProperty<DWORD>(key, NCRYPT_PCP_KEY_USAGE_POLICY_PROPERTY);
  return usage_policy.has_value() &&
         ((*usage_policy & NCRYPT_PCP_IDENTITY_KEY) != 0);
}

// ExportKey returns |key| exported in the given format or nullopt on error.
SecurityStatusOr<std::vector<uint8_t>> ExportKey(NCRYPT_KEY_HANDLE key,
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
  ASSIGN_OR_RETURN(const std::vector<uint8_t> pub_key,
                   ExportKey(key, BCRYPT_ECCPUBLIC_BLOB),
                   [](auto) { return std::nullopt; });

  // The exported key is a `BCRYPT_ECCKEY_BLOB` followed by the bytes of the
  // public key itself.
  // https://docs.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_ecckey_blob
  base::span pub_key_span = pub_key;
  if (pub_key_span.size() < sizeof(BCRYPT_ECCKEY_BLOB)) {
    return std::nullopt;
  }
  auto [header_bytes, key_bytes] =
      pub_key_span.split_at<sizeof(BCRYPT_ECCKEY_BLOB)>();
  const BCRYPT_ECCKEY_BLOB& header =
      base::subtle::reinterpret_span<const BCRYPT_ECCKEY_BLOB>(header_bytes)[0];
  // |cbKey| is documented[1] as "the length, in bytes, of the key". It is
  // not. For ECDSA public keys it is the length of a field element.
  if ((header.dwMagic != BCRYPT_ECDSA_PUBLIC_P256_MAGIC &&
       header.dwMagic != BCRYPT_ECDSA_PUBLIC_GENERIC_MAGIC) ||
      header.cbKey != 256 / 8 || key_bytes.size() != 64) {
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

  std::array<uint8_t, 1 + 32 + 32> x962 = {POINT_CONVERSION_UNCOMPRESSED};
  base::span(x962).last<64>().copy_from(key_bytes);

  return keypair::PublicKey::FromEcP256Point(x962).transform(
      [](const auto& key) { return key.ToSubjectPublicKeyInfo(); });
}

std::optional<std::vector<uint8_t>> GetRSASPKI(NCRYPT_KEY_HANDLE key) {
  ASSIGN_OR_RETURN(const std::vector<uint8_t> pub_key,
                   ExportKey(key, BCRYPT_RSAPUBLIC_BLOB),
                   [](auto) { return std::nullopt; });

  base::span pub_key_span = pub_key;
  // The exported key is a `BCRYPT_RSAKEY_BLOB` followed by the bytes of the
  // key itself.
  // https://docs.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_rsakey_blob
  if (pub_key_span.size() < sizeof(BCRYPT_RSAKEY_BLOB)) {
    return std::nullopt;
  }
  auto [header_bytes, key_bytes] =
      pub_key_span.split_at<sizeof(BCRYPT_RSAKEY_BLOB)>();
  const BCRYPT_RSAKEY_BLOB& header =
      base::subtle::reinterpret_span<const BCRYPT_RSAKEY_BLOB>(header_bytes)[0];
  if (header.Magic != static_cast<ULONG>(BCRYPT_RSAPUBLIC_MAGIC)) {
    return std::nullopt;
  }

  if (key_bytes.size() <
      base::ClampedNumeric<size_t>(header.cbPublicExp) + header.cbModulus) {
    return std::nullopt;
  }

  auto [e_bytes, rest_bytes] = key_bytes.split_at(header.cbPublicExp);
  auto n_bytes = rest_bytes.first(header.cbModulus);

  return keypair::PublicKey::FromRsaPublicKeyComponents(n_bytes, e_bytes)
      .transform([](const auto& key) { return key.ToSubjectPublicKeyInfo(); });
}

SecurityStatusOr<std::vector<uint8_t>> SignECDSA(
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

  auto [r_bytes, s_bytes] = base::span(sig).split_at<32>();
  return base::OptionalToExpected(
      ConvertEcdsaRawComponentsToDer(r_bytes, s_bytes), NTE_FAIL);
}

SecurityStatusOr<std::vector<uint8_t>> SignRSA(NCRYPT_KEY_HANDLE key,
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
                               ProviderType provider_type,
                               KeyUsage usage) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  ScopedNCryptProvider provider;
  SECURITY_STATUS status =
      NCryptOpenStorageProvider(ScopedNCryptProvider::Receiver(provider).get(),
                                GetWindowsIdentifierForProvider(provider_type),
                                /*flags=*/0);
  TPMOperation operation = usage == KeyUsage::kAttestation
                               ? TPMOperation::kWrappedAttestationKeyCreation
                               : TPMOperation::kWrappedKeyCreation;
  if (FAILED(status)) {
    LogTPMOperationError(operation, status, std::nullopt,
                         /*open_storage_provider_error=*/true);
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
    LogTPMOperationError(operation, import_status, std::nullopt);
    return ScopedNCryptKey();
  }
  return key;
}

// Builds a Windows Platform Crypto Provider (PCP) opaque key blob
// (BCRYPT_OPAQUE_KEY_BLOB) from the TPM2_Create response. This function is
// strictly TPM 2.0 only, formatting the TPM2_Create outputs into a
// PCP_KEY_BLOB_WIN8 structure with pcpType = 2 (PCPTYPE_TPM20).
//
// The binary layout corresponds to the PCP_KEY_BLOB_WIN8 structure used by the
// Microsoft Platform Crypto Provider for TPM 2.0 keys. See:
// https://github.com/microsoft/TSS.MSR/tree/main/PCPTool.v11
std::vector<uint8_t> BuildWrappedAttestationKey(
    const tpm::CreateResponse& response) {
  // Layout for BCRYPT_OPAQUE_KEY_BLOB under Windows 8+ for TPM 2.0 keys.
  // See
  // https://raw.githack.com/microsoft/TSS.MSR/master/PCPTool.v11/Using%20the%20Windows%208%20Platform%20Crypto%20Provider%20and%20Associated%20TPM%20Functionality.pdf#page=25
  struct PCP_KEY_BLOB_WIN8 {
    DWORD magic = 0x4D504350;  // 'MPCP'
    DWORD cbHeader = sizeof(PCP_KEY_BLOB_WIN8);
    DWORD pcpType = 2;  // PCP_TYPE_TPM20
    DWORD flags = 0;
    ULONG cbPublic = 0;
    ULONG cbPrivate = 0;
    ULONG cbMigrationPublic = 0;
    ULONG cbMigrationPrivate = 0;
    ULONG cbPolicyDigestList = 0;
    ULONG cbPCRBinding = 0;
    ULONG cbPCRDigest = 0;
    ULONG cbEncryptedSecret = 0;
    ULONG cbTpm12HostageBlob = 0;
  };

  const PCP_KEY_BLOB_WIN8 header{
      .flags = NCRYPT_PCP_IDENTITY_KEY,
      .cbPublic = base::checked_cast<ULONG>(response.out_public.size()),
      .cbPrivate = base::checked_cast<ULONG>(response.out_private.size()),
  };

  std::vector<uint8_t> wrapped_key(header.cbHeader + header.cbPublic +
                                   header.cbPrivate);
  base::SpanWriter<uint8_t> writer(wrapped_key);
  writer.Write(base::byte_span_from_ref(header));
  writer.Write(response.out_public);
  writer.Write(response.out_private);
  CHECK_EQ(writer.remaining(), 0u);

  return wrapped_key;
}

tpm::SignatureErrorOr<void> VerifyAndLogTpmSignature(
    base::span<const uint8_t> spki,
    base::span<const uint8_t> statement,
    base::span<const uint8_t> signature_blob) {
  ASSIGN_OR_RETURN(tpm::SignatureAlgorithms algs,
                   tpm::GetSignatureAlgorithms(signature_blob));
  base::UmaHistogramSparse(
      "Crypto.TPMOperation.Win.TpmCertifyVerify.SignatureAlgorithm",
      std::to_underlying(algs.sig_alg));
  base::UmaHistogramSparse(
      "Crypto.TPMOperation.Win.TpmCertifyVerify.HashAlgorithm",
      std::to_underlying(algs.hash_alg));

  return tpm::VerifySignature(spki, statement, signature_blob);
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

// Dynamically loading tbs.dll prevents the browser from crashing on startup
// if the Windows TPM Base Services are missing or disabled.
bool IsTbsAvailable() {
  static const bool is_available = [] {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

    // Resolve all delay-loaded imports for tbs.dll on the first call to
    // prevent failed loads being treated as a fatal failure later, which
    // can happen in rare cases due to missing or corrupted DLL file.
    base::expected<bool, HRESULT> load_result =
        base::win::LoadAllImportsForDllUnchecked("tbs.dll");
    bool available = load_result.value_or(false);
    base::UmaHistogramSparse(
        "Crypto.TPMOperation.Win.LoadTBSLibrary.Result",
        available
            ? S_OK
            : load_result.error_or(HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND)));
    return available;
  }();
  return is_available;
}

bool IsTpm20Available() {
  if (!IsTbsAvailable()) {
    return false;
  }
  TPM_DEVICE_INFO tpm_info{};
  TBS_RESULT result = ::Tbsi_GetDeviceInfo(sizeof(tpm_info), &tpm_info);
  return result == TBS_SUCCESS && tpm_info.tpmVersion >= TPM_VERSION_20;
}

// Maps a TPM operation (represented by a TPM command) to a TPMOperation enum.
//
// NOTE: Right now only restricted signing keys directly issue commands to the
// TPM. Regular signing keys are completely supported by the higher level NCrypt
// library. The mapping here needs to be changed should this cease to be the
// case.
std::optional<TPMOperation> TpmCommandToOperation(tpm::TpmCommand command) {
  switch (command) {
    case tpm::TpmCommand::kCertify:
      return TPMOperation::kKeyCertification;
    case tpm::TpmCommand::kCreate:
      return TPMOperation::kNewAttestationKeyCreation;
    case tpm::TpmCommand::kSign:
      return TPMOperation::kRestrictedMessageSigning;

    case tpm::TpmCommand::kHash:
    case tpm::TpmCommand::kHashSequenceStart:
    case tpm::TpmCommand::kSequenceComplete:
    case tpm::TpmCommand::kSequenceUpdate:
      return TPMOperation::kMessageHashing;
    case tpm::TpmCommand::kFlushContext:
      return std::nullopt;
  }

  NOTREACHED();
}

void LogTpmExtractPropertyResult(
    tpm::TpmCommand command,
    SECURITY_STATUS status,
    SignatureVerifier::SignatureAlgorithm algorithm) {
  base::UmaHistogramSparse(
      absl::StrFormat("Crypto.TPMOperation.Win.Tpm%vExtractProperty.Result",
                      command),
      status);
  if (auto op = TpmCommandToOperation(command)) {
    LogTPMOperationError(*op, status, algorithm);
  }
}

std::optional<TBS_HCONTEXT> GetTbsContext(
    NCRYPT_KEY_HANDLE key_handle,
    tpm::TpmCommand command,
    SignatureVerifier::SignatureAlgorithm algorithm) {
  auto log_extract_property_error = [&](SECURITY_STATUS status) {
    LogTpmExtractPropertyResult(command, status, algorithm);
    return std::nullopt;
  };

  ASSIGN_OR_RETURN(NCRYPT_PROV_HANDLE prov_handle,
                   GetNCryptProperty<NCRYPT_PROV_HANDLE>(
                       key_handle, NCRYPT_PROVIDER_HANDLE_PROPERTY),
                   log_extract_property_error);

  ASSIGN_OR_RETURN(TBS_HCONTEXT h_context,
                   GetNCryptProperty<TBS_HCONTEXT>(
                       prov_handle, NCRYPT_PCP_PLATFORMHANDLE_PROPERTY),
                   log_extract_property_error);

  return h_context;
}

std::optional<uint32_t> GetTpmPlatformHandle(
    NCRYPT_KEY_HANDLE key_handle,
    tpm::TpmCommand command,
    SignatureVerifier::SignatureAlgorithm algorithm) {
  return base::OptionalFromExpected(
      GetNCryptProperty<uint32_t>(key_handle,
                                  NCRYPT_PCP_PLATFORMHANDLE_PROPERTY)
          .transform_error([&](SECURITY_STATUS status) {
            LogTpmExtractPropertyResult(command, status, algorithm);
            return status;
          }));
}

std::optional<std::vector<uint8_t>> SubmitTbsCommand(
    TBS_HCONTEXT h_context,
    tpm::TpmCommand command,
    base::span<const uint8_t> cmd,
    size_t max_resp_size,
    SignatureVerifier::SignatureAlgorithm algorithm) {
  // A max_resp_size buffer handles the maximum expected TPM response.
  // Heap-allocating it protects the local stack from potential buffer
  // overflow vulnerabilities in the OS API.
  std::vector<uint8_t> resp(max_resp_size);
  UINT32 resp_len = resp.size();
  TBS_RESULT tbs_result = ::Tbsip_Submit_Command(
      h_context, TBS_COMMAND_LOCALITY_ZERO, TBS_COMMAND_PRIORITY_NORMAL,
      cmd.data(), cmd.size(), resp.data(), &resp_len);

  // Overwriting tbs_result safely catches buggy API returns that indicate
  // more bytes were written than the buffer size, preventing false "Success"
  // codes from polluting UMA metrics.
  if (tbs_result == TBS_SUCCESS && resp_len > resp.size()) {
    tbs_result = TBS_E_INSUFFICIENT_BUFFER;
  }

  if (tbs_result != TBS_SUCCESS) {
    base::UmaHistogramSparse("Crypto.TPMOperation.Win.TbsSubmitCommand.Error",
                             tbs_result);
    ASSIGN_OR_RETURN(TPMOperation op, TpmCommandToOperation(command));
    LogTPMOperationError(op, tbs_result, algorithm);
    return std::nullopt;
  }

  resp.resize(resp_len);
  return resp;
}

template <typename T>
std::optional<T> ToOptionalAndRecordParseMetrics(
    tpm::TpmParseErrorOr<T> parsed_or_error) {
  auto parse_error = parsed_or_error.error_or(
      tpm::TpmParseError(tpm::kNoTpmParseErrorForMetrics));
  base::UmaHistogramEnumeration(
      absl::StrFormat("Crypto.TPMOperation.Win.Tpm%vParse.Result", T::kCommand),
      parse_error.type);
  base::UmaHistogramSparse(
      absl::StrFormat("Crypto.TPMOperation.Win.Tpm%vResponse.TpmResponseCode",
                      T::kCommand),
      parse_error.tpm_error_code.value_or(0));
  return base::OptionalFromExpected(std::move(parsed_or_error));
}

// Maximum buffer size for a TPM2B_MAX_BUFFER structure (e.g. TPM2_Hash data
// payload).
constexpr size_t kMaxTpmHashBufferSize = 1024;

// Maximum expected response buffer size for TPM commands (e.g. TPM2_Sign and
// TPM2_Certify).
constexpr size_t kMaxTpmResponseSize = 4096;

// Holds the digest and validation ticket produced by hashing data with the TPM.
struct HashResult {
  std::vector<uint8_t> digest;
  std::vector<uint8_t> validation_ticket;
};

// Extracts the hash algorithm (`crypto::hash::HashKind`) from a
// `SignatureVerifier::SignatureAlgorithm`.
constexpr hash::HashKind ToHashKind(
    SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case SignatureVerifier::RSA_PKCS1_SHA1:
      return hash::kSha1;
    case SignatureVerifier::RSA_PKCS1_SHA256:
    case SignatureVerifier::ECDSA_SHA256:
    case SignatureVerifier::RSA_PSS_SHA256:
      return hash::kSha256;
  }
  NOTREACHED();
}

// Hashes data using either single-shot TPM2_Hash (if data <= 1024 bytes) or
// streaming TPM sequence commands (TPM2_HashSequenceStart, TPM2_SequenceUpdate,
// TPM2_SequenceComplete) for larger buffers.
std::optional<HashResult> HashDataSlowly(
    TBS_HCONTEXT h_context,
    base::span<const uint8_t> data,
    SignatureVerifier::SignatureAlgorithm algorithm) {
  const hash::HashKind hash_kind = ToHashKind(algorithm);
  if (data.size() <= kMaxTpmHashBufferSize) {
    std::vector<uint8_t> hash_cmd = tpm::BuildHashCommand(data, hash_kind);

    ASSIGN_OR_RETURN(
        std::vector<uint8_t> hash_resp,
        SubmitTbsCommand(h_context, tpm::TpmCommand::kHash, hash_cmd,
                         kMaxTpmResponseSize, algorithm));

    ASSIGN_OR_RETURN(
        tpm::HashResponse hash_parsed,
        ToOptionalAndRecordParseMetrics(tpm::ParseHashResponse(hash_resp)));

    return HashResult{
        .digest = std::move(hash_parsed.digest),
        .validation_ticket = std::move(hash_parsed.validation_ticket),
    };
  }

  // Multi-part hashing sequence for payloads larger than 1024 bytes.
  // 1. TPM2_HashSequenceStart
  std::vector<uint8_t> start_cmd =
      tpm::BuildHashSequenceStartCommand(hash_kind);

  ASSIGN_OR_RETURN(
      std::vector<uint8_t> start_resp,
      SubmitTbsCommand(h_context, tpm::TpmCommand::kHashSequenceStart,
                       start_cmd, kMaxTpmResponseSize, algorithm));

  ASSIGN_OR_RETURN(tpm::HashSequenceStartResponse start_parsed,
                   ToOptionalAndRecordParseMetrics(
                       tpm::ParseHashSequenceStartResponse(start_resp)));

  uint32_t sequence_handle = start_parsed.sequence_handle;

  // TPM2_HashSequenceStart allocates a transient sequence handle in the TPM's
  // volatile memory. If an error occurs before the sequence completes, we
  // must flush the context via TPM2_FlushContext to prevent leaking limited
  // TPM RAM resources (which would eventually cause TPM_RC_MEMORY /
  // TPM_RC_HANDLES). On success, TPM2_SequenceComplete automatically frees the
  // handle, so we cancel the cleanup guard.
  absl::Cleanup flush_guard = [h_context, sequence_handle, algorithm] {
    if (auto resp =
            SubmitTbsCommand(h_context, tpm::TpmCommand::kFlushContext,
                             tpm::BuildFlushContextCommand(sequence_handle),
                             kMaxTpmResponseSize, algorithm)) {
      ToOptionalAndRecordParseMetrics(tpm::ParseFlushContextResponse(*resp));
    }
  };

  // 2. Loop TPM2_SequenceUpdate for all chunks while remaining > 1024 bytes.
  base::span<const uint8_t> remaining = data;
  while (remaining.size() > kMaxTpmHashBufferSize) {
    auto chunk = remaining.take_first<kMaxTpmHashBufferSize>();

    std::vector<uint8_t> update_cmd =
        tpm::BuildSequenceUpdateCommand(sequence_handle, chunk);

    ASSIGN_OR_RETURN(
        std::vector<uint8_t> update_resp,
        SubmitTbsCommand(h_context, tpm::TpmCommand::kSequenceUpdate,
                         update_cmd, kMaxTpmResponseSize, algorithm));

    RETURN_IF_ERROR(ToOptionalAndRecordParseMetrics(
        tpm::ParseSequenceUpdateResponse(update_resp)));
  }

  // 3. TPM2_SequenceComplete with remaining data (<= 1024 bytes).
  std::vector<uint8_t> complete_cmd =
      tpm::BuildSequenceCompleteCommand(sequence_handle, remaining);

  ASSIGN_OR_RETURN(
      std::vector<uint8_t> complete_resp,
      SubmitTbsCommand(h_context, tpm::TpmCommand::kSequenceComplete,
                       complete_cmd, kMaxTpmResponseSize, algorithm));

  ASSIGN_OR_RETURN(tpm::SequenceCompleteResponse complete_parsed,
                   ToOptionalAndRecordParseMetrics(
                       tpm::ParseSequenceCompleteResponse(complete_resp)));

  std::move(flush_guard).Cancel();
  return HashResult{
      .digest = std::move(complete_parsed.digest),
      .validation_ticket = std::move(complete_parsed.validation_ticket),
  };
}

// AttestationKeyWin wraps an Attestation Identity Key (AIK) on Windows. Given
// the lack of support for restricted TPM signing keys in the Windows NCrypt
// APIs, this implementation talks to the TPM directly via TBS (TPM Base
// Services) and constructs the low-level TPM commands manually.
class AttestationKeyWin : public WinKeyImpl<UnexportableAttestationKey> {
 public:
  AttestationKeyWin(ProviderType provider_type, KeyDetails details)
      : WinKeyImpl(provider_type, std::move(details)) {}

  // UnexportableSigningKey:
  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    // 1. Check TBS availability
    if (!IsTbsAvailable()) {
      return std::nullopt;
    }

    // 2. Extract Provider Context and TPM handles
    ASSIGN_OR_RETURN(TBS_HCONTEXT h_context,
                     GetTbsContext(GetNCryptKeyHandle(), tpm::TpmCommand::kSign,
                                   Algorithm()));

    ASSIGN_OR_RETURN(uint32_t sign_handle,
                     GetTpmPlatformHandle(GetNCryptKeyHandle(),
                                          tpm::TpmCommand::kSign, Algorithm()));

    // 3. Hash Data (single-shot or streaming sequence)
    ASSIGN_OR_RETURN(HashResult hash_result,
                     HashDataSlowly(h_context, data, Algorithm()));

    // 4. Submit TPM2_Sign Command
    // Attestation Identity Keys (AIKs) are restricted signing keys whose
    // signature scheme is fixed in the key's public template upon creation.
    // Per TPM 2.0 Part 3 Section 19.2 (TPM2_Sign), `inScheme` is set to
    // `TPM_ALG_NULL` for restricted keys so the TPM uses the scheme defined in
    // the key object itself. Setting `inScheme` to `TPM_ALG_NULL` specifies no
    // scheme-specific parameters, meaning `hash_alg` is ignored in the
    // serialized `TPMT_SIG_SCHEME`.
    std::vector<uint8_t> sign_cmd = tpm::BuildSignCommand(
        sign_handle, hash_result.digest, hash_result.validation_ticket);

    ASSIGN_OR_RETURN(
        std::vector<uint8_t> sign_resp,
        SubmitTbsCommand(h_context, tpm::TpmCommand::kSign, sign_cmd,
                         kMaxTpmResponseSize, Algorithm()));

    // 5. Parse TPM2_Sign Response
    ASSIGN_OR_RETURN(
        tpm::SignResponse sign_parsed,
        ToOptionalAndRecordParseMetrics(tpm::ParseSignResponse(sign_resp)));

    // 6. Normalize signature format (DER for ECDSA, raw for RSA)
    return tpm::ParseTpmSignature(sign_parsed.signature);
  }

  bool SupportsTls13() override {
    // TODO(crbug.com/530828835): Implement.
    NOTIMPLEMENTED();
    return false;
  }

  // UnexportableAttestationKey:
  std::optional<AttestationStatement> CertifySlowly(
      const UnexportableSigningKey& signing_key,
      base::span<const uint8_t> challenge) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    // 1. Check TBS availability
    if (!IsTbsAvailable()) {
      return std::nullopt;
    }

    // 2. Extract Provider Context and TPM handles
    ASSIGN_OR_RETURN(TBS_HCONTEXT h_context,
                     GetTbsContext(GetNCryptKeyHandle(),
                                   tpm::TpmCommand::kCertify, Algorithm()));

    ASSIGN_OR_RETURN(
        uint32_t object_handle,
        GetTpmPlatformHandle(signing_key.GetNCryptKeyHandle(),
                             tpm::TpmCommand::kCertify, Algorithm()));

    ASSIGN_OR_RETURN(
        uint32_t sign_handle,
        GetTpmPlatformHandle(GetNCryptKeyHandle(), tpm::TpmCommand::kCertify,
                             Algorithm()));

    // 3. Construct Command
    const auto qualifying_data = hash::Sha256(challenge);
    std::vector<uint8_t> cmd =
        tpm::BuildCertifyCommand(object_handle, sign_handle, qualifying_data);

    // 4. Submit Command
    ASSIGN_OR_RETURN(std::vector<uint8_t> resp,
                     SubmitTbsCommand(h_context, tpm::TpmCommand::kCertify, cmd,
                                      kMaxTpmResponseSize, Algorithm()));

    // 5. Parse in Rust by going through the C++ shim.
    ASSIGN_OR_RETURN(tpm::CertifyResponse parsed,
                     ToOptionalAndRecordParseMetrics(
                         tpm::ParseCertifyResponse(resp, qualifying_data)));

    // 6. Verify in C++. C++ supports a wider range of signature algorithms than
    // Rust.
    base::UmaHistogramEnumeration(
        "Crypto.TPMOperation.Win.TpmCertifyVerify.Result",
        VerifyAndLogTpmSignature(GetSubjectPublicKeyInfo(), parsed.statement,
                                 parsed.signature)
            .error_or(tpm::kNoSignatureErrorForMetrics));

    return AttestationStatement{
        .format = AttestationStatement::kTpm,
        .statement = std::move(parsed.statement),
        .signature = std::move(parsed.signature),
    };
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

  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
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
        return nullptr;
      }
    }

    ASSIGN_OR_RETURN(SignatureVerifier::SignatureAlgorithm algo,
                     GetBestSupported(provider.get(), acceptable_algorithms),
                     [] { return nullptr; });

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
        return nullptr;
      }

      if (provider_type_ == ProviderType::kTPM &&
          algo == SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256) {
        // TPM 2.0 RSA keys created via the Platform Crypto Provider default to
        // SHA-1 for signing if left unset. Restrict the key to SHA-256 instead.
        RETURN_IF_ERROR(
            SetNCryptProperty(key.get(),
                              NCRYPT_PCP_RSA_SCHEME_HASH_ALG_PROPERTY,
                              static_cast<DWORD>(tpm::TPM_ALG_SHA256)),
            [&](SECURITY_STATUS status) {
              LogTPMOperationError(TPMOperation::kNewKeyCreation, status, algo);
              return nullptr;
            });
      }

      if (FAILED(NCryptFinalizeKey(key.get(), NCRYPT_SILENT_FLAG))) {
        return nullptr;
      }
    }
    if (provider_type_ == ProviderType::kTPM) {
      ASSIGN_OR_RETURN(key_id, ExportKey(key.get(), BCRYPT_OPAQUE_KEY_BLOB),
                       [&](SECURITY_STATUS status) {
                         LogTPMOperationError(TPMOperation::kWrappedKeyExport,
                                              status, algo);
                         return nullptr;
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
        }(),
        [] { return nullptr; });

    KeyDetails key_details{std::move(key), std::move(key_id), std::move(spki),
                           algo};
    switch (algo) {
      case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        return std::make_unique<ECDSASigningKey>(provider_type_,
                                                 std::move(key_details));
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        return std::make_unique<RSASigningKey>(provider_type_,
                                               std::move(key_details));
      default:
        return nullptr;
    }
  }

  // Generates a TPM 2.0 Attestation Identity Key (AIK) by submitting a raw
  // TPM2_Create command via TBS and importing the resulting opaque key blob
  // into the Windows Platform Crypto Provider (PCP).
  //
  // Windows CNG does not support creating AIKs with modern parameters (e.g.,
  // ECDSA P-256 with SHA-256) directly through NCryptCreatePersistedKey.
  // Instead, we construct and issue TPM2_Create directly under the Storage Root
  // Key (SRK), format the TPM2B_PUBLIC and TPM2B_PRIVATE into a
  // BCRYPT_OPAQUE_KEY_BLOB (PCP_KEY_BLOB_WIN8), and import it via
  // NCryptImportKey.
  std::unique_ptr<UnexportableAttestationKey> GenerateAttestationKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    if (provider_type_ != ProviderType::kTPM || !IsTbsAvailable() ||
        !IsTpm20Available()) {
      return nullptr;
    }

    // 1. Open the Platform Crypto Provider and select the best supported
    // algorithm.
    ScopedNCryptProvider provider;
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      SECURITY_STATUS status = NCryptOpenStorageProvider(
          ScopedNCryptProvider::Receiver(provider).get(),
          GetWindowsIdentifierForProvider(provider_type_), /*flags=*/0);
      if (FAILED(status)) {
        LogTPMOperationError(TPMOperation::kNewAttestationKeyCreation, status,
                             std::nullopt,
                             /*open_storage_provider_error=*/true);
        return nullptr;
      }
    }

    ASSIGN_OR_RETURN(SignatureVerifier::SignatureAlgorithm algo,
                     GetBestSupported(provider.get(), acceptable_algorithms),
                     [] { return nullptr; });

    // 2. Extract the underlying TBS context handle from the provider.
    ASSIGN_OR_RETURN(TBS_HCONTEXT h_context,
                     GetNCryptProperty<TBS_HCONTEXT>(
                         provider.get(), NCRYPT_PCP_PLATFORMHANDLE_PROPERTY),
                     [&](SECURITY_STATUS status) {
                       LogTPMOperationError(
                           TPMOperation::kNewAttestationKeyCreation, status,
                           algo);
                       return nullptr;
                     });

    // 3. Construct and submit the TPM2_Create command to generate the AIK under
    // the Storage Root Key (SRK).
    ASSIGN_OR_RETURN(
        std::vector<uint8_t> create_cmd,
        tpm::BuildCreateAikCommand(std::to_underlying(GetSrkHandleFor(algo)),
                                   ToSignatureKind(algo)),
        [&] {
          LogTPMOperationError(TPMOperation::kNewAttestationKeyCreation,
                               NTE_NOT_SUPPORTED, algo);
          return nullptr;
        });

    ASSIGN_OR_RETURN(std::vector<uint8_t> create_resp,
                     SubmitTbsCommand(h_context, tpm::TpmCommand::kCreate,
                                      create_cmd, kMaxTpmResponseSize, algo),
                     [] { return nullptr; });

    // 4. Parse the TPM2_Create response to extract the public and private
    // key areas.
    ASSIGN_OR_RETURN(
        tpm::CreateResponse parsed_create,
        ToOptionalAndRecordParseMetrics(tpm::ParseCreateResponse(create_resp)),
        [] { return nullptr; });

    // 5. Build a BCRYPT_OPAQUE_KEY_BLOB (PCP_KEY_BLOB_WIN8) from the
    // TPM2_Create output and import it to obtain a functional key handle.
    return FromWrappedAttestationKeySlowly(
        BuildWrappedAttestationKey(parsed_create));
  }

  std::optional<KeyDetails> FromWrappedKeyImpl(
      base::span<const uint8_t> wrapped,
      KeyUsage usage) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);

    ScopedNCryptKey key = LoadWrappedKey(wrapped, provider_type_, usage);
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

ScopedNCryptKey DuplicatePlatformKeyHandle(const UnexportableSigningKey& key) {
  return LoadWrappedKey(
      key.GetWrappedKey(),
      key.IsHardwareBacked() ? ProviderType::kTPM : ProviderType::kSoftware,
      IsIdentityKey(key.GetNCryptKeyHandle()) ? KeyUsage::kAttestation
                                              : KeyUsage::kSigning);
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
