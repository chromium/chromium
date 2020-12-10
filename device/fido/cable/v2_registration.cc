// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/cable/v2_registration.h"

#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/device_event_log/device_event_log.h"
#include "components/gcm_driver/gcm_app_handler.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {
namespace cablev2 {
namespace authenticator {

namespace {

static const char kFCMAppId[] = "chrome.android.features.cablev2_authenticator";
static const char kFCMSenderId[] = "141743603694";

class FCMHandler : public gcm::GCMAppHandler, public Registration {
 public:
  FCMHandler(instance_id::InstanceIDDriver* instance_id_driver,
             base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
                 event_callback)
      : event_callback_(std::move(event_callback)),
        instance_id_driver_(instance_id_driver),
        instance_id_(instance_id_driver->GetInstanceID(kFCMAppId)) {
    // Registering with FCM on platforms other than Android could cause large
    // number of new registrations with the FCM service. Thus this code does not
    // compile on other platforms. Check with //components/gcm_driver owners
    // before changing this.
#if !defined(OS_ANDROID)
    CHECK(false) << "Do not use outside of Android.";
#endif

    gcm::GCMDriver* const gcm_driver = instance_id_->gcm_driver();
    CHECK(gcm_driver->GetAppHandler(kFCMAppId) == nullptr);
    instance_id_->gcm_driver()->AddAppHandler(kFCMAppId, this);
  }

  ~FCMHandler() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    instance_id_->gcm_driver()->RemoveAppHandler(kFCMAppId);
    instance_id_driver_->RemoveInstanceID(kFCMAppId);
  }

  // Registration:

  void PrepareContactID() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    instance_id_->GetToken(
        kFCMSenderId, instance_id::kGCMScope,
        /*time_to_live=*/base::TimeDelta(),
        /*flags=*/{},
        base::BindOnce(&FCMHandler::GetTokenComplete, base::Unretained(this)));
  }

  void RotateContactID() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    registration_token_.reset();
    instance_id_->DeleteToken(kFCMSenderId, instance_id::kGCMScope,
                              base::BindOnce(&FCMHandler::DeleteTokenComplete,
                                             base::Unretained(this)));
  }

  base::Optional<std::vector<uint8_t>> contact_id() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!registration_token_) {
      return base::nullopt;
    }
    return std::vector<uint8_t>(registration_token_->begin(),
                                registration_token_->end());
  }

  // GCMAppHandler:

  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(app_id, kFCMAppId);
    DCHECK_EQ(message.sender_id, kFCMSenderId);

    if (app_id != kFCMAppId || message.sender_id != kFCMSenderId) {
      FIDO_LOG(ERROR) << "Discarding FCM message from " << message.sender_id;
      return;
    }

    base::Optional<std::unique_ptr<Registration::Event>> event =
        MessageToEvent(message.data);
    if (!event) {
      FIDO_LOG(ERROR) << "Failed to decode FCM message. Ignoring.";
      return;
    }

    event_callback_.Run(std::move(*event));
  }

  void ShutdownHandler() override {}
  void OnStoreReset() override {}
  void OnMessagesDeleted(const std::string& app_id) override {}
  void OnSendError(
      const std::string& app_id,
      const gcm::GCMClient::SendErrorDetails& send_error_details) override {}
  void OnSendAcknowledged(const std::string& app_id,
                          const std::string& message_id) override {}
  void OnMessageDecryptionFailed(const std::string& app_id,
                                 const std::string& message_id,
                                 const std::string& error_message) override {}
  bool CanHandle(const std::string& app_id) const override { return false; }

 private:
  void GetTokenComplete(const std::string& token,
                        instance_id::InstanceID::Result result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (result != instance_id::InstanceID::SUCCESS) {
      FIDO_LOG(ERROR) << "Getting FCM token failed: "
                      << static_cast<int>(result);
      return;
    }

    FIDO_LOG(ERROR) << __func__ << " " << token;
    registration_token_ = token;
  }

  void DeleteTokenComplete(instance_id::InstanceID::Result result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (result != instance_id::InstanceID::SUCCESS) {
      FIDO_LOG(ERROR) << "Deleting FCM token failed: "
                      << static_cast<int>(result);
    }

    PrepareContactID();
  }

  static base::Optional<std::unique_ptr<Registration::Event>> MessageToEvent(
      const gcm::MessageData& data) {
    auto event = std::make_unique<Registration::Event>();
    gcm::MessageData::const_iterator it = data.find("caBLE.tunnelID");
    if (it == data.end() ||
        !base::HexStringToSpan(it->second, event->tunnel_id)) {
      return base::nullopt;
    }

    it = data.find("caBLE.routingID");
    if (it == data.end() ||
        !base::HexStringToSpan(it->second, event->routing_id)) {
      return base::nullopt;
    }

    std::vector<uint8_t> payload_bytes;
    it = data.find("caBLE.clientPayload");
    if (it == data.end() ||
        !base::HexStringToBytes(it->second, &payload_bytes)) {
      return base::nullopt;
    }

    base::Optional<cbor::Value> payload = cbor::Reader::Read(payload_bytes);
    if (!payload || !payload->is_map()) {
      return base::nullopt;
    }

    const cbor::Value::MapValue& map = payload->GetMap();
    cbor::Value::MapValue::const_iterator cbor_it = map.find(cbor::Value(1));
    if (cbor_it == map.end() || !cbor_it->second.is_bytestring()) {
      return base::nullopt;
    }
    event->pairing_id = cbor_it->second.GetBytestring();

    if (!fido_parsing_utils::CopyCBORBytestring(&event->client_nonce, map, 2)) {
      return base::nullopt;
    }

    return event;
  }

  base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
      event_callback_;
  instance_id::InstanceIDDriver* const instance_id_driver_;
  instance_id::InstanceID* const instance_id_;
  base::Optional<std::string> registration_token_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

Registration::~Registration() = default;
Registration::Event::Event() = default;
Registration::Event::~Event() = default;

std::unique_ptr<Registration> Register(
    instance_id::InstanceIDDriver* instance_id_driver,
    base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
        event_callback) {
  return std::make_unique<FCMHandler>(instance_id_driver,
                                      std::move(event_callback));
}

}  // namespace authenticator
}  // namespace cablev2
}  // namespace device
