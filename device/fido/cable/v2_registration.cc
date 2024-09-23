// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/cable/v2_registration.h"

#include "base/memory/raw_ptr.h"
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
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace device {
namespace cablev2 {
namespace authenticator {

namespace {

static const char kFCMAppId[] = "chrome.android.features.cablev2_authenticator";
static const char kFCMSyncAppId[] =
    "chrome.android.features.cablev2_authenticator_sync";
static const char kFCMSenderId[] = "141743603694";

class FCMHandler : public gcm::GCMAppHandler, public Registration {
 public:
  FCMHandler(instance_id::InstanceIDDriver* instance_id_driver,
             Registration::Type type,
             base::OnceCallback<void()> on_ready,
             base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
                 event_callback)
      : type_(type),
        ready_callback_(std::move(on_ready)),
        event_callback_(std::move(event_callback)),
        instance_id_driver_(instance_id_driver),
        instance_id_(instance_id_driver->GetInstanceID(app_id())) {
    // Registering with FCM on platforms other than Android could cause large
    // number of new registrations with the FCM service. Thus this code does not
    // compile on other platforms. Check with //components/gcm_driver owners
    // before changing this.
#if !BUILDFLAG(IS_ANDROID)
    CHECK(false) << "Do not use outside of Android.";
#endif

    gcm::GCMDriver* const gcm_driver = instance_id_->gcm_driver();
    CHECK(gcm_driver->GetAppHandler(app_id()) == nullptr);
    instance_id_->gcm_driver()->AddAppHandler(app_id(), this);
  }

  ~FCMHandler() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    instance_id_->gcm_driver()->RemoveAppHandler(app_id());
    instance_id_driver_->RemoveInstanceID(app_id());
  }

  // Registration:

  void PrepareContactID() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (registration_token_pending_) {
      return;
    }
    registration_token_pending_ = true;

    instance_id_->GetToken(
        kFCMSenderId, instance_id::kGCMScope,
        /*time_to_live=*/base::TimeDelta(),
        // This flag causes high-priority messages to be processed immediately
        // rather than deferred until the device is not dozing.
        {instance_id::InstanceID::Flags::kBypassScheduler},
        base::BindOnce(&FCMHandler::GetTokenComplete, base::Unretained(this)));
  }

  void RotateContactID() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    registration_token_.reset();
    instance_id_->DeleteToken(kFCMSenderId, instance_id::kGCMScope,
                              base::BindOnce(&FCMHandler::DeleteTokenComplete,
                                             base::Unretained(this)));
  }

  std::optional<std::vector<uint8_t>> contact_id() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!registration_token_) {
      return std::nullopt;
    }
    return std::vector<uint8_t>(registration_token_->begin(),
                                registration_token_->end());
  }

  // GCMAppHandler:

  void OnMessage(const std::string& app_id,
                 const gcm::IncomingMessage& message) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(app_id, this->app_id());
    DCHECK_EQ(message.sender_id, kFCMSenderId);

    if (app_id != this->app_id() || message.sender_id != kFCMSenderId) {
      FIDO_LOG(ERROR) << "Discarding FCM message from " << message.sender_id;
      return;
    }

    std::optional<std::unique_ptr<Registration::Event>> event =
        MessageToEvent(message.data, type_);
    if (!event) {
      FIDO_LOG(ERROR) << "Failed to decode FCM message. Ignoring.";
      return;
    }

    if (type_ == Type::LINKING) {
      event.value()->contact_id = contact_id();
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
  const char* app_id() const {
    switch (type_) {
      case Type::LINKING:
        return kFCMAppId;
      case Type::SYNC:
        return kFCMSyncAppId;
    }
  }

  void GetTokenComplete(const std::string& token,
                        instance_id::InstanceID::Result result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    registration_token_pending_ = false;

    if (result != instance_id::InstanceID::SUCCESS) {
      FIDO_LOG(ERROR) << "Getting FCM token failed: "
                      << static_cast<int>(result);
      return;
    }

    registration_token_ = token;

    if (ready_callback_) {
      std::move(ready_callback_).Run();
    }
  }

  void DeleteTokenComplete(instance_id::InstanceID::Result result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (result != instance_id::InstanceID::SUCCESS) {
      FIDO_LOG(ERROR) << "Deleting FCM token failed: "
                      << static_cast<int>(result);
    }

    PrepareContactID();
  }

  static std::optional<std::unique_ptr<Registration::Event>> MessageToEvent(
      const gcm::MessageData& data,
      Type source) {
    auto event = std::make_unique<Registration::Event>();
    event->source = source;

    gcm::MessageData::const_iterator it = data.find("caBLE.tunnelID");
    if (it == data.end() ||
        !base::HexStringToSpan(it->second, event->tunnel_id)) {
      return std::nullopt;
    }

    it = data.find("caBLE.routingID");
    if (it == data.end() ||
        !base::HexStringToSpan(it->second, event->routing_id)) {
      return std::nullopt;
    }

    std::vector<uint8_t> payload_bytes;
    it = data.find("caBLE.clientPayload");
    if (it == data.end() ||
        !base::HexStringToBytes(it->second, &payload_bytes)) {
      return std::nullopt;
    }

    std::optional<cbor::Value> payload = cbor::Reader::Read(payload_bytes);
    if (!payload || !payload->is_map()) {
      return std::nullopt;
    }

    const cbor::Value::MapValue& map = payload->GetMap();
    cbor::Value::MapValue::const_iterator cbor_it = map.find(cbor::Value(1));
    if (cbor_it == map.end() || !cbor_it->second.is_bytestring()) {
      return std::nullopt;
    }
    const std::vector<uint8_t>& pairing_id = cbor_it->second.GetBytestring();
    if (pairing_id.size() != event->pairing_id.size()) {
      return std::nullopt;
    }
    memcpy(event->pairing_id.data(), pairing_id.data(),
           event->pairing_id.size());

    if (!fido_parsing_utils::CopyCBORBytestring(&event->client_nonce, map, 2)) {
      return std::nullopt;
    }

    cbor_it = map.find(cbor::Value(3));
    if (cbor_it == map.end() || !cbor_it->second.is_string()) {
      return std::nullopt;
    }
    const std::string& request_type_str = cbor_it->second.GetString();
    if (request_type_str == "mc") {
      event->request_type = FidoRequestType::kMakeCredential;
    } else if (request_type_str == "ga") {
      event->request_type = FidoRequestType::kGetAssertion;
    } else {
      return std::nullopt;
    }

    return event;
  }

  const Type type_;
  base::OnceCallback<void()> ready_callback_;
  base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
      event_callback_;
  const raw_ptr<instance_id::InstanceIDDriver> instance_id_driver_;
  const raw_ptr<instance_id::InstanceID> instance_id_;
  bool registration_token_pending_ = false;
  std::optional<std::string> registration_token_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace

Registration::~Registration() = default;
Registration::Event::Event() = default;
Registration::Event::~Event() = default;

// static
std::unique_ptr<Registration::Event> Registration::Event::FromSerialized(
    base::span<const uint8_t> in) {
  auto e = std::make_unique<Event>();
  uint8_t source, request_type;
  CBS cbs;
  CBS_init(&cbs, in.data(), in.size());

  if (!CBS_get_u8(&cbs, &source) ||        //
      !CBS_get_u8(&cbs, &request_type) ||  //
      !CBS_copy_bytes(&cbs, e->tunnel_id.data(), e->tunnel_id.size()) ||
      !CBS_copy_bytes(&cbs, e->routing_id.data(), e->routing_id.size()) ||
      !CBS_copy_bytes(&cbs, e->pairing_id.data(), e->pairing_id.size()) ||
      !CBS_copy_bytes(&cbs, e->client_nonce.data(), e->client_nonce.size())) {
    return nullptr;
  }

  if ((source != static_cast<uint8_t>(Type::LINKING) &&
       source != static_cast<uint8_t>(Type::SYNC)) ||
      (request_type != static_cast<uint8_t>(FidoRequestType::kGetAssertion) &&
       request_type !=
           static_cast<uint8_t>(FidoRequestType::kMakeCredential))) {
    return nullptr;
  }
  e->source = static_cast<Type>(source);
  e->request_type = static_cast<FidoRequestType>(request_type);

  switch (e->source) {
    case Type::LINKING:
    case Type::SYNC:
      // This exists to catch additions to this enum as they need to be handled
      // in the conditional, above.
      break;
  }

  switch (e->request_type) {
    case FidoRequestType::kMakeCredential:
    case FidoRequestType::kGetAssertion:
      // This exists to catch additions to this enum as they need to be handled
      // in the conditional, above.
      break;
  }

  if (CBS_len(&cbs) > 0) {
    if (e->source == Type::SYNC) {
      return nullptr;
    }
    e->contact_id.emplace(CBS_data(&cbs), CBS_data(&cbs) + CBS_len(&cbs));
  }

  return e;
}

std::optional<std::vector<uint8_t>> Registration::Event::Serialize() {
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), /*initial_capacity=*/512) ||
      !CBB_add_u8(cbb.get(), static_cast<uint8_t>(this->source)) ||
      !CBB_add_u8(cbb.get(), static_cast<uint8_t>(this->request_type)) ||
      !CBB_add_bytes(cbb.get(), this->tunnel_id.data(),
                     this->tunnel_id.size()) ||
      !CBB_add_bytes(cbb.get(), this->routing_id.data(),
                     this->routing_id.size()) ||
      !CBB_add_bytes(cbb.get(), this->pairing_id.data(),
                     this->pairing_id.size()) ||
      !CBB_add_bytes(cbb.get(), this->client_nonce.data(),
                     this->client_nonce.size()) ||
      (this->contact_id && !CBB_add_bytes(cbb.get(), this->contact_id->data(),
                                          this->contact_id->size()))) {
    return std::nullopt;
  }

  uint8_t* serialized_bytes;
  size_t serialized_bytes_len;
  if (!CBB_finish(cbb.get(), &serialized_bytes, &serialized_bytes_len)) {
    return std::nullopt;
  }
  const std::vector<uint8_t> ret(serialized_bytes,
                                 serialized_bytes + serialized_bytes_len);
  OPENSSL_free(serialized_bytes);

  return ret;
}

std::unique_ptr<Registration> Register(
    instance_id::InstanceIDDriver* instance_id_driver,
    Registration::Type type,
    base::OnceCallback<void()> on_ready,
    base::RepeatingCallback<void(std::unique_ptr<Registration::Event>)>
        event_callback) {
  return std::make_unique<FCMHandler>(
      instance_id_driver, type, std::move(on_ready), std::move(event_callback));
}

}  // namespace authenticator
}  // namespace cablev2
}  // namespace device
