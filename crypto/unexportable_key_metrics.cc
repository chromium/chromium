// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key_metrics.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"

namespace crypto {

namespace {

#if BUILDFLAG(IS_APPLE)
// Appended to the keychain access group to form the application tag of the
// benchmark keys. Keys under this tag belong to no feature, so the benchmark
// may delete any it finds.
constexpr std::string_view kTpmMetricsApplicationTagSuffix = ".tpm-metrics";
#endif  // BUILDFLAG(IS_APPLE)

const sign::SignatureKind kAllAlgorithms[] = {
    sign::ECDSA_SHA256,
    sign::RSA_PKCS1_SHA256,
};

std::string GetHistogramSuffixForAlgo(internal::TPMSupport algo) {
  switch (algo) {
    case internal::TPMSupport::kECDSA:
      return "ECDSA";
    case internal::TPMSupport::kRSA:
      return "RSA";
    case internal::TPMSupport::kNone:
      return "";
  }
  return "";
}

void ReportUmaLatency(TPMOperation operation,
                      internal::TPMSupport algo,
                      base::TimeDelta latency) {
  std::string histogram_name = "Crypto.TPMDuration." +
                               OperationToString(operation) +
                               GetHistogramSuffixForAlgo(algo);
  base::UmaHistogramMediumTimes(histogram_name, latency);
}

void ReportUmaOperationSuccess(TPMOperation operation,
                               internal::TPMSupport algo,
                               bool status) {
  std::string histogram_name = "Crypto.TPMOperation." +
                               OperationToString(operation) +
                               GetHistogramSuffixForAlgo(algo);
  base::UmaHistogramBoolean(histogram_name, status);
}

void ReportUmaTpmOperation(TPMOperation operation,
                           internal::TPMSupport algo,
                           base::TimeDelta latency,
                           bool status) {
  ReportUmaOperationSuccess(operation, algo, status);
  if (status && operation != TPMOperation::kMessageVerify) {
    // Only report latency for successful operations
    // No latency reported for verification that is done outside of TPM
    ReportUmaLatency(operation, algo, latency);
  }
}

bool VerifySignature(sign::SignatureKind alg,
                     base::span<const uint8_t> spki,
                     base::span<const uint8_t> data,
                     base::span<const uint8_t> signature) {
  std::optional<keypair::PublicKey> public_key =
      keypair::PublicKey::FromSubjectPublicKeyInfo(spki);
  return public_key.has_value() &&
         sign::Verify(alg, *public_key, data, signature);
}

void MeasureTpmOperationsInternal(UnexportableKeyProvider::Config config) {
#if BUILDFLAG(IS_APPLE)
  // Keys on Apple platforms persist in the keychain. Give the benchmark keys
  // their own tag so that they can be told apart from keys of real features.
  config.application_tag = base::StrCat(
      {config.keychain_access_group, kTpmMetricsApplicationTagSuffix});
#endif  // BUILDFLAG(IS_APPLE)

  internal::TPMSupport supported_algo = internal::TPMSupport::kNone;
  std::unique_ptr<UnexportableKeyProvider> provider =
      GetUnexportableKeyProvider(std::move(config));
  if (!provider) {
    return;
  }

  auto algo = provider->SelectAlgorithm(kAllAlgorithms);
  if (algo) {
    switch (*algo) {
      case sign::ECDSA_SHA256:
        supported_algo = internal::TPMSupport::kECDSA;
        break;
      case sign::RSA_PKCS1_SHA256:
        supported_algo = internal::TPMSupport::kRSA;
        break;
      case sign::RSA_PKCS1_SHA1:
      case sign::RSA_PKCS1_SHA384:
      case sign::RSA_PKCS1_SHA512:
      case sign::RSA_PSS_SHA256:
      case sign::RSA_PSS_SHA384:
      case sign::RSA_PSS_SHA512:
      case sign::ECDSA_SHA1:
      case sign::ECDSA_SHA384:
      case sign::ECDSA_SHA512:
      case sign::ED25519:
      case sign::MLDSA_44:
      case sign::MLDSA_65:
      case sign::MLDSA_87:
        // Not supported for this metric.
        break;
    }
  }

  // Report if TPM is supported and best algo
  base::UmaHistogramEnumeration("Crypto.TPMSupport2", supported_algo);
  if (supported_algo == internal::TPMSupport::kNone) {
    return;
  }

#if BUILDFLAG(IS_APPLE)
  // The keys below are deleted when they go out of scope, but a browser that
  // exits mid-measurement leaves them in the keychain. Delete such leftovers
  // from earlier runs, so that they don't accumulate (crbug.com/566267325).
  if (StatefulUnexportableKeyProvider* stateful_provider =
          provider->AsStatefulUnexportableKeyProvider()) {
    stateful_provider->DeleteAllKeysSlowly();
  }
#endif  // BUILDFLAG(IS_APPLE)

  auto delete_key = [&provider](UnexportableSigningKey* key) {
    if (StatefulUnexportableKeyProvider* stateful_provider =
            provider->AsStatefulUnexportableKeyProvider()) {
      stateful_provider->DeleteWrappedKeysSlowly({key->GetWrappedKey()});
    }
    delete key;
  };

  auto wrap_delete_key =
      [delete_key]<typename KeyT>(std::unique_ptr<KeyT> key) {
        return std::unique_ptr<KeyT, decltype(delete_key)>(
            std::move(key).release(), delete_key);
      };

  base::ElapsedTimer key_creation_timer;
  auto current_key =
      wrap_delete_key(provider->GenerateSigningKeySlowly(kAllAlgorithms));
  ReportUmaTpmOperation(TPMOperation::kNewKeyCreation, supported_algo,
                        key_creation_timer.Elapsed(), current_key != nullptr);
  if (!current_key) {
    return;
  }

  base::ElapsedTimer wrapped_key_creation_timer;
  auto wrapped_key = wrap_delete_key(
      provider->FromWrappedSigningKeySlowly(current_key->GetWrappedKey()));
  ReportUmaTpmOperation(TPMOperation::kWrappedKeyCreation, supported_algo,
                        wrapped_key_creation_timer.Elapsed(),
                        wrapped_key != nullptr);

  base::ElapsedTimer attestation_key_creation_timer;
  auto attestation_key =
      wrap_delete_key(provider->GenerateAttestationKeySlowly(kAllAlgorithms));
  ReportUmaTpmOperation(
      TPMOperation::kNewAttestationKeyCreation, supported_algo,
      attestation_key_creation_timer.Elapsed(), attestation_key != nullptr);

  if (attestation_key) {
    base::ElapsedTimer wrapped_attestation_key_creation_timer;
    auto wrapped_attestation_key =
        wrap_delete_key(provider->FromWrappedAttestationKeySlowly(
            attestation_key->GetWrappedKey()));
    ReportUmaTpmOperation(TPMOperation::kWrappedAttestationKeyCreation,
                          supported_algo,
                          wrapped_attestation_key_creation_timer.Elapsed(),
                          wrapped_attestation_key != nullptr);

    base::ElapsedTimer certification_timer;
    std::optional<AttestationStatement> certification =
        attestation_key->CertifySlowly(*current_key, {5, 6, 7, 8});
    ReportUmaTpmOperation(TPMOperation::kKeyCertification, supported_algo,
                          certification_timer.Elapsed(),
                          certification.has_value());

    // Multi-part TPM hashing sequences (TPM2_HashSequenceStart,
    // TPM2_SequenceUpdate, TPM2_SequenceComplete) are used for payloads larger
    // than 1024 bytes. Use a 2048-byte buffer to ensure the streaming TPM
    // sequence path is benchmarked.
    std::array<uint8_t, 2048> msg;
    msg.fill(1);
    base::ElapsedTimer attestation_signing_timer;
    std::optional<std::vector<uint8_t>> signed_attestation_bytes =
        attestation_key->SignSlowly(msg);
    ReportUmaTpmOperation(TPMOperation::kRestrictedMessageSigning,
                          supported_algo, attestation_signing_timer.Elapsed(),
                          signed_attestation_bytes.has_value());
    if (signed_attestation_bytes.has_value()) {
      ReportUmaOperationSuccess(
          TPMOperation::kRestrictedMessageVerify, supported_algo,
          VerifySignature(attestation_key->Algorithm(),
                          attestation_key->GetSubjectPublicKeyInfo(), msg,
                          *signed_attestation_bytes));
    }
  }

  const uint8_t msg[] = {1, 2, 3, 4};
  base::ElapsedTimer message_signing_timer;
  std::optional<std::vector<uint8_t>> signed_bytes =
      current_key->SignSlowly(msg);
  ReportUmaTpmOperation(TPMOperation::kMessageSigning, supported_algo,
                        message_signing_timer.Elapsed(),
                        signed_bytes.has_value());
  if (!signed_bytes.has_value()) {
    return;
  }

  ReportUmaOperationSuccess(
      TPMOperation::kMessageVerify, supported_algo,
      VerifySignature(current_key->Algorithm(),
                      current_key->GetSubjectPublicKeyInfo(), msg,
                      *signed_bytes));
}

}  // namespace

namespace internal {

void MeasureTpmOperationsInternalForTesting() {
  MeasureTpmOperationsInternal(/*config=*/{});
}

}  // namespace internal

std::string OperationToString(TPMOperation operation) {
  switch (operation) {
    case TPMOperation::kMessageSigning:
      return "MessageSigning";
    case TPMOperation::kMessageVerify:
      return "MessageVerify";
    case TPMOperation::kNewKeyCreation:
      return "NewKeyCreation";
    case TPMOperation::kWrappedKeyCreation:
      return "WrappedKeyCreation";
    case TPMOperation::kWrappedKeyExport:
      return "WrappedKeyExport";
    case TPMOperation::kSelectAlgorithm:
      return "SelectAlgorithm";
    case TPMOperation::kKeyDeletion:
      return "KeyDeletion";
    case TPMOperation::kKeyCertification:
      return "KeyCertification";
    case TPMOperation::kNewAttestationKeyCreation:
      return "NewAttestationKeyCreation";
    case TPMOperation::kWrappedAttestationKeyCreation:
      return "WrappedAttestationKeyCreation";
    case TPMOperation::kWrappedAttestationKeyExport:
      return "WrappedAttestationKeyExport";
    case TPMOperation::kMessageHashing:
      return "MessageHashing";
    case TPMOperation::kRestrictedMessageSigning:
      return "RestrictedMessageSigning";
    case TPMOperation::kRestrictedMessageVerify:
      return "RestrictedMessageVerify";
  }
}

std::string AlgorithmToString(sign::SignatureKind algorithm) {
  switch (algorithm) {
    case sign::RSA_PKCS1_SHA1:
    case sign::RSA_PKCS1_SHA256:
    case sign::RSA_PKCS1_SHA384:
    case sign::RSA_PKCS1_SHA512:
    case sign::RSA_PSS_SHA256:
    case sign::RSA_PSS_SHA384:
    case sign::RSA_PSS_SHA512:
      return "RSA";
    case sign::ECDSA_SHA1:
    case sign::ECDSA_SHA256:
    case sign::ECDSA_SHA384:
    case sign::ECDSA_SHA512:
      return "ECDSA";
    case sign::ED25519:
      return "ED25519";
    case sign::MLDSA_44:
    case sign::MLDSA_65:
    case sign::MLDSA_87:
      return "MLDSA";
  }
}

void MaybeMeasureTpmOperations(UnexportableKeyProvider::Config config) {
  MeasureTpmOperationsInternal(std::move(config));
}

}  // namespace crypto
