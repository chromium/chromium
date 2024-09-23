// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key_metrics.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "crypto/unexportable_key.h"

namespace crypto {

namespace {

enum class KeyType {
  kHardwareKey,
  kVirtualizedKey,
};

const SignatureVerifier::SignatureAlgorithm kAllAlgorithms[] = {
    SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
    SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
};

constexpr char kTestKeyName[] = "ChromeMetricsTestKey";

// Leaving HW empty will keep the existing metric as is today.
std::string GetHistogramPrefixForKeyType(KeyType type) {
  switch (type) {
    case KeyType::kHardwareKey:
      return "";
    case KeyType::kVirtualizedKey:
      return "Virtual.";
  }
}

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

internal::TPMType GetSupportedTpm(internal::TPMSupport hw,
                                  internal::TPMSupport virt) {
  if (hw != internal::TPMSupport::kNone &&
      virt != internal::TPMSupport::kNone) {
    return internal::TPMType::kBoth;
  }

  if (hw != internal::TPMSupport::kNone) {
    return internal::TPMType::kHW;
  }

  // This is not expected
  if (virt != internal::TPMSupport::kNone) {
    return internal::TPMType::kVirtual;
  }

  return internal::TPMType::kNone;
}

void ReportUmaLatency(TPMOperation operation,
                      internal::TPMSupport algo,
                      base::TimeDelta latency,
                      KeyType type = KeyType::kHardwareKey) {
  std::string histogram_name =
      "Crypto.TPMDuration." + GetHistogramPrefixForKeyType(type) +
      OperationToString(operation) + GetHistogramSuffixForAlgo(algo);
  base::UmaHistogramMediumTimes(histogram_name, latency);
}

void ReportUmaOperationSuccess(TPMOperation operation,
                               internal::TPMSupport algo,
                               bool status,
                               KeyType type = KeyType::kHardwareKey) {
  std::string histogram_name =
      "Crypto.TPMOperation." + GetHistogramPrefixForKeyType(type) +
      OperationToString(operation) + GetHistogramSuffixForAlgo(algo);
  base::UmaHistogramBoolean(histogram_name, status);
}

void ReportUmaTpmOperation(TPMOperation operation,
                           internal::TPMSupport algo,
                           base::TimeDelta latency,
                           bool status,
                           KeyType type = KeyType::kHardwareKey) {
  ReportUmaOperationSuccess(operation, algo, status, type);
  if (status && operation != TPMOperation::kMessageVerify) {
    // Only report latency for successful operations
    // No latency reported for verification that is done outside of TPM
    ReportUmaLatency(operation, algo, latency, type);
  }
}

internal::TPMSupport MeasureVirtualTpmOperations() {
  internal::TPMSupport supported_virtual_algo = internal::TPMSupport::kNone;
  std::unique_ptr<VirtualUnexportableKeyProvider> virtual_provider =
      GetVirtualUnexportableKeyProvider_DO_NOT_USE_METRICS_ONLY();

  if (!virtual_provider) {
    return supported_virtual_algo;
  }

  auto algo = virtual_provider->SelectAlgorithm(kAllAlgorithms);
  if (algo) {
    switch (*algo) {
      case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        supported_virtual_algo = internal::TPMSupport::kECDSA;
        break;
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        supported_virtual_algo = internal::TPMSupport::kRSA;
        break;
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
      case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
        // Not supported for this metric.
        break;
    }
  }

  // Report if virtual TPM is supported and best algo
  base::UmaHistogramEnumeration("Crypto.VirtualKeySupport",
                                supported_virtual_algo);

  base::ElapsedTimer key_creation_timer;
  std::unique_ptr<VirtualUnexportableSigningKey> current_key =
      virtual_provider->GenerateSigningKey(kAllAlgorithms, kTestKeyName);
  ReportUmaTpmOperation(TPMOperation::kNewKeyCreation, supported_virtual_algo,
                        key_creation_timer.Elapsed(), current_key != nullptr,
                        KeyType::kVirtualizedKey);
  if (!current_key) {
    // Report no support if keys cannot be created, Windows appears to always
    // mark the keys as available in SelectAlgorithm.
    return internal::TPMSupport::kNone;
  }

  base::ElapsedTimer open_key_timer;
  std::string key_name = current_key->GetKeyName();
  std::unique_ptr<VirtualUnexportableSigningKey> opened_key =
      virtual_provider->FromKeyName(key_name);
  // Re-using TPMOperation::kWrappedKeyCreation for restoring keys even though
  // there are no wrapped keys involved.
  ReportUmaTpmOperation(TPMOperation::kWrappedKeyCreation,
                        supported_virtual_algo, open_key_timer.Elapsed(),
                        opened_key != nullptr, KeyType::kVirtualizedKey);

  const uint8_t msg[] = {1, 2, 3, 4};
  base::ElapsedTimer message_signing_timer;
  std::optional<std::vector<uint8_t>> signed_bytes = current_key->Sign(msg);
  ReportUmaTpmOperation(TPMOperation::kMessageSigning, supported_virtual_algo,
                        message_signing_timer.Elapsed(),
                        signed_bytes.has_value(), KeyType::kVirtualizedKey);

  if (signed_bytes.has_value()) {
    crypto::SignatureVerifier verifier;
    bool verify_init =
        verifier.VerifyInit(current_key->Algorithm(), signed_bytes.value(),
                            current_key->GetSubjectPublicKeyInfo());
    if (verify_init) {
      verifier.VerifyUpdate(msg);
      bool verify_final = verifier.VerifyFinal();
      ReportUmaOperationSuccess(TPMOperation::kMessageVerify,
                                supported_virtual_algo, verify_final,
                                KeyType::kVirtualizedKey);
    } else {
      ReportUmaOperationSuccess(TPMOperation::kMessageVerify,
                                supported_virtual_algo, verify_init,
                                KeyType::kVirtualizedKey);
    }
  }

  current_key.get()->DeleteKey();
  return supported_virtual_algo;
}

void MeasureTpmOperationsInternal(UnexportableKeyProvider::Config config) {
  internal::TPMSupport supported_algo = internal::TPMSupport::kNone;
  std::unique_ptr<UnexportableKeyProvider> provider =
      GetUnexportableKeyProvider(std::move(config));
  if (!provider) {
    base::UmaHistogramEnumeration("Crypto.TPMSupportType", supported_algo);
    return;
  }

  auto algo = provider->SelectAlgorithm(kAllAlgorithms);
  if (algo) {
    switch (*algo) {
      case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        supported_algo = internal::TPMSupport::kECDSA;
        break;
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        supported_algo = internal::TPMSupport::kRSA;
        break;
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
      case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
        // Not supported for this metric.
        break;
    }
  }

  internal::TPMSupport supported_virtual_algo = MeasureVirtualTpmOperations();
  base::UmaHistogramEnumeration(
      "Crypto.TPMSupportType",
      GetSupportedTpm(supported_algo, supported_virtual_algo));

  // Report if TPM is supported and best algo
  base::UmaHistogramEnumeration("Crypto.TPMSupport2", supported_algo);
  if (supported_algo == internal::TPMSupport::kNone) {
    return;
  }

  auto delete_key = [&provider](UnexportableSigningKey* key) {
    provider->DeleteSigningKeySlowly(key->GetWrappedKey());
    delete key;
  };
  base::ElapsedTimer key_creation_timer;
  std::unique_ptr<UnexportableSigningKey, decltype(delete_key)> current_key(
      provider->GenerateSigningKeySlowly(kAllAlgorithms).release(), delete_key);
  ReportUmaTpmOperation(TPMOperation::kNewKeyCreation, supported_algo,
                        key_creation_timer.Elapsed(), current_key != nullptr);
  if (!current_key) {
    return;
  }

  base::ElapsedTimer wrapped_key_creation_timer;
  std::unique_ptr<UnexportableSigningKey, decltype(delete_key)> wrapped_key(
      provider->FromWrappedSigningKeySlowly(current_key->GetWrappedKey())
          .release(),
      delete_key);
  ReportUmaTpmOperation(TPMOperation::kWrappedKeyCreation, supported_algo,
                        wrapped_key_creation_timer.Elapsed(),
                        wrapped_key != nullptr);

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

  crypto::SignatureVerifier verifier;
  bool verify_init =
      verifier.VerifyInit(current_key->Algorithm(), signed_bytes.value(),
                          current_key->GetSubjectPublicKeyInfo());
  if (verify_init) {
    verifier.VerifyUpdate(msg);
    bool verify_final = verifier.VerifyFinal();
    ReportUmaOperationSuccess(TPMOperation::kMessageVerify, supported_algo,
                              verify_final);
  } else {
    ReportUmaOperationSuccess(TPMOperation::kMessageVerify, supported_algo,
                              verify_init);
  }
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
  }
}

std::string AlgorithmToString(SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
    case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
    case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
      return "RSA";
    case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      return "ECDSA";
  }
}

void MaybeMeasureTpmOperations(UnexportableKeyProvider::Config config) {
  static BASE_FEATURE(kTpmLatencyMetrics, "TpmLatencyMetrics",
                      base::FEATURE_ENABLED_BY_DEFAULT);
  if (base::FeatureList::IsEnabled(kTpmLatencyMetrics)) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&MeasureTpmOperationsInternal, std::move(config)));
  }
}

}  // namespace crypto
