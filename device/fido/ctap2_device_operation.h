// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP2_DEVICE_OPERATION_H_
#define DEVICE_FIDO_CTAP2_DEVICE_OPERATION_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "components/cbor/diagnostic_writer.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/device_operation.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"

namespace device {

// Ctap2DeviceOperation performs a single request--response operation on a CTAP2
// device. The |Request| class must implement a static |EncodeToCBOR| method
// that returns a pair of |CtapRequestCommand| and an optional CBOR |Value|. The
// response will be parsed to CBOR and then further parsed into a |Response|
// using a provided callback.
template <class Request, class Response>
class Ctap2DeviceOperation : public DeviceOperation<Request, Response> {
 public:
  // DeviceResponseCallback is either called with a |kSuccess| and a |Response|
  // object, or else is called with a value other than |kSuccess| and
  // |nullopt|.
  using DeviceResponseCallback =
      base::OnceCallback<void(CtapDeviceResponseCode, std::optional<Response>)>;
  // DeviceResponseParser converts a generic CBOR structure into an
  // operation-specific response. If the response didn't have a payload then the
  // argument will be |nullopt|. The parser should return |nullopt| on error.
  using DeviceResponseParser = base::OnceCallback<std::optional<Response>(
      const std::optional<cbor::Value>&)>;
  // CBORPathPredicate takes a vector of CBOR |Value|s that are map keys and
  // returns true if the string at that location may validly be truncated.
  // For example, the path of the string "bar" in {"x": {"y": "foo",
  // "z": "bar"}} is ["x", "z"].
  //
  // It's a function pointer rather than a callback to emphasise that the result
  // should be stateless and based only on the static structure of the expected
  // message.
  typedef bool (*CBORPathPredicate)(
      const std::vector<const cbor::Value*>& path);

  Ctap2DeviceOperation(FidoDevice* device,
                       Request request,
                       DeviceResponseCallback callback,
                       DeviceResponseParser device_response_parser,
                       CBORPathPredicate string_fixup_predicate)
      : DeviceOperation<Request, Response>(device,
                                           std::move(request),
                                           std::move(callback)),
        device_response_parser_(std::move(device_response_parser)),
        string_fixup_predicate_(string_fixup_predicate) {}

  Ctap2DeviceOperation(const Ctap2DeviceOperation&) = delete;
  Ctap2DeviceOperation& operator=(const Ctap2DeviceOperation&) = delete;

  ~Ctap2DeviceOperation() override = default;

  void Start() override {
    std::pair<CtapRequestCommand, std::optional<cbor::Value>> request(
        AsCTAPRequestValuePair(this->request()));
    std::vector<uint8_t> request_bytes;

    // TODO: it would be nice to see which device each request is going to, but
    // that breaks every mock test because they aren't expecting a call to
    // GetId().
    if (request.second) {
      FIDO_LOG(DEBUG) << "<- " << request.first << " "
                      << cbor::DiagnosticWriter::Write(*request.second);
      std::optional<std::vector<uint8_t>> cbor_bytes =
          cbor::Writer::Write(*request.second);
      DCHECK(cbor_bytes);
      request_bytes = std::move(*cbor_bytes);
    } else {
      FIDO_LOG(DEBUG) << "<- " << request.first << " (no payload)";
    }

    request_bytes.insert(request_bytes.begin(),
                         static_cast<uint8_t>(request.first));

    this->token_ = this->device()->DeviceTransact(
        std::move(request_bytes),
        base::BindOnce(&Ctap2DeviceOperation::OnResponseReceived,
                       weak_factory_.GetWeakPtr()));
  }

  // Cancel requests that the operation be canceled. This is safe to call at any
  // time but may not be effective because the operation may have already
  // completed or the device may not support cancelation. Even if canceled, the
  // callback will still be invoked, albeit perhaps with a status of
  // |kCtap2ErrKeepAliveCancel|.
  void Cancel() override {
    if (this->token_) {
      FIDO_LOG(DEBUG) << "<- (cancel)";
      this->device()->Cancel(*this->token_);
      this->token_.reset();
    }
  }

  void OnResponseReceived(std::optional<std::vector<uint8_t>> device_response) {
    this->token_.reset();

    // TODO: it would be nice to see which device each response is coming from,
    // but that breaks every mock test because they aren't expecting a call to
    // GetId().
    if (!device_response || device_response->empty()) {
      FIDO_LOG(ERROR) << "-> (error reading)";
      std::move(this->callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, std::nullopt);
      return;
    }

    auto response_code = GetResponseCode(*device_response);
    if (response_code != CtapDeviceResponseCode::kSuccess) {
      if (response_code == CtapDeviceResponseCode::kCtap2ErrInvalidCBOR) {
        FIDO_LOG(DEBUG) << "-> (Unknown CTAP2 error code "
                        << static_cast<int>(device_response->at(0)) << ")";
      } else {
        FIDO_LOG(DEBUG) << "-> (CTAP2 error code " << response_code << ")";
      }
      std::move(this->callback()).Run(response_code, std::nullopt);
      return;
    }
    DCHECK(!device_response->empty());

    std::optional<cbor::Value> cbor;
    std::optional<Response> response;
    base::span<const uint8_t> cbor_bytes(*device_response);
    cbor_bytes = cbor_bytes.subspan(1);

    if (!cbor_bytes.empty()) {
      cbor::Reader::DecoderError error;
      cbor::Reader::Config config;
      config.error_code_out = &error;
      if (string_fixup_predicate_) {
        config.allow_invalid_utf8 = true;
      }

      cbor = cbor::Reader::Read(cbor_bytes, config);
      if (!cbor) {
        FIDO_LOG(ERROR) << "-> (CBOR parse error '"
                        << cbor::Reader::ErrorCodeToString(error)
                        << "' from raw message "
                        << base::HexEncode(device_response.value()) << ")";
        std::move(this->callback())
            .Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR, std::nullopt);
        return;
      }

      if (string_fixup_predicate_) {
        cbor = FixInvalidUTF8(std::move(*cbor), string_fixup_predicate_);
        if (!cbor) {
          FIDO_LOG(ERROR)
              << "-> (CBOR with unfixable UTF-8 errors from raw message "
              << base::HexEncode(device_response.value()) << ")";
          std::move(this->callback())
              .Run(CtapDeviceResponseCode::kCtap2ErrInvalidCBOR, std::nullopt);
          return;
        }
      }

      response = std::move(std::move(device_response_parser_).Run(cbor));
      if (response) {
        FIDO_LOG(DEBUG) << "-> " << cbor::DiagnosticWriter::Write(*cbor);
      } else {
        FIDO_LOG(ERROR) << "-> (rejected CBOR structure) "
                        << cbor::DiagnosticWriter::Write(*cbor);
      }
    } else {
      response =
          std::move(std::move(device_response_parser_).Run(std::nullopt));
      if (response) {
        FIDO_LOG(DEBUG) << "-> (empty payload)";
      } else {
        FIDO_LOG(ERROR) << "-> (rejected empty payload)";
      }
    }

    if (!response) {
      response_code = CtapDeviceResponseCode::kCtap2ErrInvalidCBOR;
    }
    std::move(this->callback()).Run(response_code, std::move(response));
  }

 private:
  DeviceResponseParser device_response_parser_;
  const CBORPathPredicate string_fixup_predicate_;
  base::WeakPtrFactory<Ctap2DeviceOperation> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP2_DEVICE_OPERATION_H_
