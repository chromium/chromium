// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/transact.h"

#include "base/check.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/enclave/attestation.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "device/fido/enclave/enclave_websocket_client.h"
#include "device/fido/network_context_factory.h"
#include "device/fido/public/features.h"

namespace device::enclave {

namespace {

std::string_view TransactionTypeToSuffix(EnclaveTransactionTypeForUMA type) {
  switch (type) {
    case EnclaveTransactionTypeForUMA::kDeviceRegister:
      return ".DeviceRegister";
    case EnclaveTransactionTypeForUMA::kKeysWrapSecrets:
      return ".KeysWrapSecrets";
    case EnclaveTransactionTypeForUMA::kDeviceForget:
      return ".DeviceForget";
    case EnclaveTransactionTypeForUMA::kRecoveryKeyStoreWrapPINAndSecret:
      return ".RecoveryKeyStoreWrapPINAndSecret";
    case EnclaveTransactionTypeForUMA::kRecoveryKeyStoreRewrapPIN:
      return ".RecoveryKeyStoreRewrapPIN";
    case EnclaveTransactionTypeForUMA::kRecoveryKeyStoreWrapPINAndKeysWrap:
      return ".RecoveryKeyStoreWrapPINAndKeysWrap";
    case EnclaveTransactionTypeForUMA::kPasskeyAssert:
      return ".PasskeyAssert";
    case EnclaveTransactionTypeForUMA::kPasskeyCreate:
      return ".PasskeyCreate";
  }
  NOTREACHED();
}

class Transaction : public EnclaveTransaction {
 public:
  Transaction(
      const EnclaveIdentity& enclave,
      cbor::Value request,
      EnclaveTransactionTypeForUMA transaction_type,
      SigningCallback signing_callback,
      base::OnceCallback<void(base::expected<cbor::Value, TransactError>)>
          callback)
      : transaction_type_(transaction_type),
        request_(std::move(request)),
        signing_callback_(std::move(signing_callback)),
        callback_(std::move(callback)),
        handshake_(std::nullopt, enclave.public_key, std::nullopt) {}

  ~Transaction() override = default;

  void set_client(std::unique_ptr<EnclaveWebSocketClient> client) {
    client_ = std::move(client);
  }

  void RecordTransactionResult(EnclaveTransactionResult result) {
    transaction_result_ = result;
  }

  void RecordMetrics() {
    std::string_view cmd_suffix = TransactionTypeToSuffix(transaction_type_);
    std::string detailed_prefix =
        base::StrCat({"WebAuthentication.EnclaveTransaction", cmd_suffix});
    std::string aggregate_prefix = "WebAuthentication.EnclaveTransaction";

    CHECK(!start_time_.is_null());
    base::TimeDelta latency = base::TimeTicks::Now() - start_time_;

    if (transaction_result_) {
      base::UmaHistogramEnumeration(base::StrCat({detailed_prefix, ".Result"}),
                                    *transaction_result_);
      base::UmaHistogramEnumeration(base::StrCat({aggregate_prefix, ".Result"}),
                                    *transaction_result_);
    }

    base::UmaHistogramMediumTimes(base::StrCat({detailed_prefix, ".Latency"}),
                                  latency);
    base::UmaHistogramMediumTimes(base::StrCat({aggregate_prefix, ".Latency"}),
                                  latency);

    if (request_size_) {
      base::UmaHistogramCounts1M(
          base::StrCat({detailed_prefix, ".RequestSize"}), *request_size_);
      base::UmaHistogramCounts1M(
          base::StrCat({aggregate_prefix, ".RequestSize"}), *request_size_);
    }

    if (response_size_) {
      base::UmaHistogramCounts1M(
          base::StrCat({detailed_prefix, ".ResponseSize"}), *response_size_);
      base::UmaHistogramCounts1M(
          base::StrCat({aggregate_prefix, ".ResponseSize"}), *response_size_);
    }
  }

  void StartInternal() {
    start_time_ = base::TimeTicks::Now();
    client_->Write(handshake_.BuildInitialMessage());
  }

  void Start() {
    if (base::FeatureList::IsEnabled(
            device::kWebAuthnEnclaveAuthenticatorDelay)) {
      // Unretained is fine because this is a development flag.
      timer_.Start(
          FROM_HERE, base::Seconds(5),
          base::BindOnce(&Transaction::StartInternal, base::Unretained(this)));
      return;
    }
    StartInternal();
  }

  void OnData(device::enclave::EnclaveWebSocketClient::SocketStatus status,
              std::vector<uint8_t> data) {
    if (status != EnclaveWebSocketClient::SocketStatus::kOk) {
      FIDO_LOG(ERROR) << "Enclave WebSocket connection failed";
      RecordTransactionResult(EnclaveTransactionResult::kWebSocketError);
      OnTransactionComplete(base::unexpected(TransactError::kWebSocketError));
      return;
    }

    if (!done_handshake_) {
      if (!CompleteHandshake(data)) {
        RecordTransactionResult(EnclaveTransactionResult::kHandshakeFailed);
        OnTransactionComplete(
            base::unexpected(TransactError::kHandshakeFailed));
        return;
      }

      FIDO_LOG(EVENT) << "<- "
                      << cbor::DiagnosticWriter::Write(
                             RedactEnclaveRequest(request_));
      BuildCommandRequestBody(std::move(request_), std::move(signing_callback_),
                              *handshake_hash_,
                              base::BindOnce(&Transaction::RequestReady,
                                             weak_factory_.GetWeakPtr()));
    } else {
      response_size_ = data.size();
      std::vector<uint8_t> plaintext;
      if (!crypter_->Decrypt(data, &plaintext)) {
        FIDO_LOG(ERROR) << "Failed to decrypt enclave response";
        RecordTransactionResult(EnclaveTransactionResult::kDecryptionFailed);
        OnTransactionComplete(base::unexpected(TransactError::kOther));
        return;
      }

      std::optional<cbor::Value> response = cbor::Reader::Read(plaintext);
      if (!response) {
        FIDO_LOG(ERROR) << "Failed to parse enclave response";
        RecordTransactionResult(EnclaveTransactionResult::kParseFailure);
        OnTransactionComplete(base::unexpected(TransactError::kOther));
        return;
      }

      FIDO_LOG(EVENT) << "-> "
                      << cbor::DiagnosticWriter::Write(
                             RedactEnclaveResponse(*response));
      if (!response->is_map()) {
        RecordTransactionResult(EnclaveTransactionResult::kParseFailure);
        OnTransactionComplete(base::unexpected(TransactError::kOther));
        return;
      }

      const cbor::Value::MapValue& map = response->GetMap();
      const cbor::Value::MapValue::const_iterator ok_it =
          map.find(cbor::Value("ok"));
      if (ok_it == map.end()) {
        const cbor::Value::MapValue::const_iterator err_it =
            map.find(cbor::Value("err"));
        if (err_it != map.end() && err_it->second.is_integer()) {
          int code = err_it->second.GetInteger();
          if (code == static_cast<int>(TransactError::kUnknownClient)) {
            RecordTransactionResult(EnclaveTransactionResult::kUnknownClient);
            OnTransactionComplete(
                base::unexpected(TransactError::kUnknownClient));
            return;
          }
          if (code == static_cast<int>(TransactError::kMissingKey)) {
            RecordTransactionResult(EnclaveTransactionResult::kMissingKey);
            OnTransactionComplete(base::unexpected(TransactError::kMissingKey));
            return;
          }
          if (code ==
              static_cast<int>(TransactError::kSignatureVerificationFailed)) {
            RecordTransactionResult(
                EnclaveTransactionResult::kSignatureVerificationFailed);
            OnTransactionComplete(
                base::unexpected(TransactError::kSignatureVerificationFailed));
            return;
          }
          // Any other code deliberately falls through to
          // kUnknownServiceError.
        }
        RecordTransactionResult(EnclaveTransactionResult::kOtherError);
        OnTransactionComplete(
            base::unexpected(TransactError::kUnknownServiceError));
        return;
      }

      RecordTransactionResult(EnclaveTransactionResult::kSuccess);
      OnTransactionComplete(base::ok(ok_it->second.Clone()));
    }
  }

  base::WeakPtr<Transaction> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  void OnTransactionComplete(
      base::expected<cbor::Value, TransactError> result) {
    RecordMetrics();
    client_.reset();
    std::move(callback_).Run(std::move(result));
  }

  void RequestReady(std::optional<std::vector<uint8_t>> request) {
    if (!callback_) {
      FIDO_LOG(EVENT)
          << "Signing callback completed after transaction was finalized.";
      return;
    }
    if (!request) {
      FIDO_LOG(EVENT)
          << "Signing failed, potentially due to the user canceling";
      OnTransactionComplete(base::unexpected(TransactError::kSigningFailed));
      return;
    }

    if (!crypter_->Encrypt(&request.value())) {
      FIDO_LOG(ERROR) << "Failed to encrypt message to enclave";
      OnTransactionComplete(base::unexpected(TransactError::kOther));
      return;
    }
    request_size_ = request->size();
    client_->Write(*request);
  }

  bool CompleteHandshake(const std::vector<uint8_t>& data) {
    base::span<const uint8_t> response(data);
    if (response.size() < cablev2::HandshakeInitiator::kResponseSize) {
      FIDO_LOG(ERROR) << "Enclave handshake response too short";
      return false;
    }

    auto attestation =
        response.subspan(cablev2::HandshakeInitiator::kResponseSize);
    response = response.first<cablev2::HandshakeInitiator::kResponseSize>();

    cablev2::HandshakeResult result = handshake_.ProcessResponse(response);
    if (!result) {
      FIDO_LOG(ERROR) << "Enclave handshake failed";
      return false;
    }

    if (base::FeatureList::IsEnabled(device::kWebAuthnEnclaveAttestation)) {
      auto attestation_result =
          ProcessAttestation(attestation, /*handshake_hash=*/result->second);
      if (!attestation_result.has_value()) {
        FIDO_LOG(ERROR) << "Attestation checking failed: "
                        << attestation_result.error();
        return false;
      }
      // TODO: establish minimum firmware versions and enforce that
      // `attestation_result` meets that bar. Likely want an UMA to measure
      // attestation failure rates (which should be zero).
    }

    crypter_ = std::move(result->first);
    handshake_hash_ = result->second;
    done_handshake_ = true;

    return true;
  }

  const EnclaveTransactionTypeForUMA transaction_type_;
  cbor::Value request_;

  SigningCallback signing_callback_;
  base::OnceCallback<void(base::expected<cbor::Value, TransactError>)>
      callback_;
  cablev2::HandshakeInitiator handshake_;
  std::unique_ptr<EnclaveWebSocketClient> client_;
  std::unique_ptr<cablev2::Crypter> crypter_;
  std::optional<std::array<uint8_t, 32>> handshake_hash_;
  bool done_handshake_ = false;
  base::TimeTicks start_time_;
  std::optional<size_t> request_size_;
  std::optional<size_t> response_size_;
  std::optional<EnclaveTransactionResult> transaction_result_;

  // Timer for `kWebAuthnEnclaveAuthenticatorDelay` dev flag.
  base::OneShotTimer timer_;

  base::WeakPtrFactory<Transaction> weak_factory_{this};
};

}  // namespace

std::unique_ptr<EnclaveTransaction> Transact(
    NetworkContextFactory network_context_factory,
    const EnclaveIdentity& enclave,
    std::string access_token,
    std::optional<std::string> reauthentication_token,
    cbor::Value request,
    EnclaveTransactionTypeForUMA transaction_type,
    SigningCallback signing_callback,
    base::OnceCallback<void(base::expected<cbor::Value, TransactError>)>
        callback) {
  auto transaction = std::make_unique<Transaction>(
      enclave, std::move(request), transaction_type,
      std::move(signing_callback), std::move(callback));

  transaction->set_client(std::make_unique<EnclaveWebSocketClient>(
      enclave.url, std::move(access_token), std::move(reauthentication_token),
      std::move(network_context_factory),
      base::BindRepeating(&Transaction::OnData, transaction->GetWeakPtr())));

  transaction->Start();
  return transaction;
}

}  // namespace device::enclave
