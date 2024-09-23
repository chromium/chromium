// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "google_apis/gcm/base/mcs_util.h"

#include <stddef.h>

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/clock.h"
#include "base/time/time.h"

namespace gcm {

namespace {

// Type names corresponding to MCSProtoTags. Useful for identifying what type
// of MCS protobuf is contained within a google::protobuf::MessageLite object.
// WARNING: must match the order in MCSProtoTag.
const char* const kProtoNames[] = {
  "mcs_proto.HeartbeatPing",
  "mcs_proto.HeartbeatAck",
  "mcs_proto.LoginRequest",
  "mcs_proto.LoginResponse",
  "mcs_proto.Close",
  "mcs_proto.MessageStanza",
  "mcs_proto.PresenceStanza",
  "mcs_proto.IqStanza",
  "mcs_proto.DataMessageStanza",
  "mcs_proto.BatchPresenceStanza",
  "mcs_proto.StreamErrorStanza",
  "mcs_proto.HttpRequest",
  "mcs_proto.HttpResponse",
  "mcs_proto.BindAccountRequest",
  "mcs_proto.BindAccountResponse",
  "mcs_proto.TalkMetadata"
};
static_assert(std::size(kProtoNames) == kNumProtoTypes,
              "Proto Names Must Include All Tags");

const char kLoginId[] = "chrome-";
const char kLoginDomain[] = "mcs.android.com";
const char kLoginDeviceIdPrefix[] = "android-";
const char kLoginSettingDefaultName[] = "new_vc";
const char kLoginSettingDefaultValue[] = "1";

// Maximum amount of time to save an unsent outgoing message for.
const int kMaxTTLSeconds = 24 * 60 * 60;  // 1 day.

}  // namespace

std::unique_ptr<mcs_proto::LoginRequest> BuildLoginRequest(
    uint64_t auth_id,
    uint64_t auth_token,
    const std::string& version_string) {
  // Create a hex encoded auth id for the device id field.
  std::string auth_id_hex;
  auth_id_hex = base::StringPrintf("%" PRIx64, auth_id);

  std::string auth_id_str = base::NumberToString(auth_id);
  std::string auth_token_str = base::NumberToString(auth_token);

  std::unique_ptr<mcs_proto::LoginRequest> login_request(
      new mcs_proto::LoginRequest());

  login_request->set_adaptive_heartbeat(false);
  login_request->set_auth_service(mcs_proto::LoginRequest::ANDROID_ID);
  login_request->set_auth_token(auth_token_str);
  login_request->set_id(kLoginId + version_string);
  login_request->set_domain(kLoginDomain);
  login_request->set_device_id(kLoginDeviceIdPrefix + auth_id_hex);
  login_request->set_network_type(1);
  login_request->set_resource(auth_id_str);
  login_request->set_user(auth_id_str);
  login_request->set_use_rmq2(true);

  login_request->add_setting();
  login_request->mutable_setting(0)->set_name(kLoginSettingDefaultName);
  login_request->mutable_setting(0)->set_value(kLoginSettingDefaultValue);
  return login_request;
}

std::unique_ptr<mcs_proto::IqStanza> BuildStreamAck() {
  std::unique_ptr<mcs_proto::IqStanza> stream_ack_iq(new mcs_proto::IqStanza());
  stream_ack_iq->set_type(mcs_proto::IqStanza::SET);
  stream_ack_iq->set_id("");
  stream_ack_iq->mutable_extension()->set_id(kStreamAck);
  stream_ack_iq->mutable_extension()->set_data("");
  return stream_ack_iq;
}

std::unique_ptr<mcs_proto::IqStanza> BuildSelectiveAck(
    const std::vector<std::string>& acked_ids) {
  std::unique_ptr<mcs_proto::IqStanza> selective_ack_iq(
      new mcs_proto::IqStanza());
  selective_ack_iq->set_type(mcs_proto::IqStanza::SET);
  selective_ack_iq->set_id("");
  selective_ack_iq->mutable_extension()->set_id(kSelectiveAck);
  mcs_proto::SelectiveAck selective_ack;
  for (size_t i = 0; i < acked_ids.size(); ++i)
    selective_ack.add_id(acked_ids[i]);
  selective_ack_iq->mutable_extension()->set_data(
      selective_ack.SerializeAsString());
  return selective_ack_iq;
}

// Utility method to build a google::protobuf::MessageLite object from a MCS
// tag.
std::unique_ptr<google::protobuf::MessageLite> BuildProtobufFromTag(
    uint8_t tag) {
  switch(tag) {
    case kHeartbeatPingTag:
      return std::unique_ptr<google::protobuf::MessageLite>(
          new mcs_proto::HeartbeatPing());
    case kHeartbeatAckTag:
      return std::unique_ptr<google::protobuf::MessageLite>(
          new mcs_proto::HeartbeatAck());
    case kLoginRequestTag:
      return std::unique_ptr<google::protobuf::MessageLite>(
          new mcs_proto::LoginRequest());
    case kLoginResponseTag:
      return std::unique_ptr<google::protobuf::MessageLite>(
          new mcs_proto::LoginResponse());
    case kCloseTag:
      return std::unique_ptr<google::protobuf::MessageLite>(
          new mcs_proto::Close());
    case kIqStanzaTag:
      return std::unique_ptr<google::protobuf::MessageLite>(
          new mcs_proto::IqStanza());
    case kDataMessageStanzaTag:
      return std::unique_ptr<google::protobuf::MessageLite>(
          new mcs_proto::DataMessageStanza());
    case kStreamErrorStanzaTag:
      return std::unique_ptr<google::protobuf::MessageLite>(
          new mcs_proto::StreamErrorStanza());
    default:
      return nullptr;
  }
}

// Utility method to extract a MCS tag from a google::protobuf::MessageLite
// object.
int GetMCSProtoTag(const google::protobuf::MessageLite& message) {
  const std::string& type_name = message.GetTypeName();
  if (type_name == kProtoNames[kHeartbeatPingTag]) {
    return kHeartbeatPingTag;
  } else if (type_name == kProtoNames[kHeartbeatAckTag]) {
    return kHeartbeatAckTag;
  } else if (type_name == kProtoNames[kLoginRequestTag]) {
    return kLoginRequestTag;
  } else if (type_name == kProtoNames[kLoginResponseTag]) {
    return kLoginResponseTag;
  } else if (type_name == kProtoNames[kCloseTag]) {
    return kCloseTag;
  } else if (type_name == kProtoNames[kIqStanzaTag]) {
    return kIqStanzaTag;
  } else if (type_name == kProtoNames[kDataMessageStanzaTag]) {
    return kDataMessageStanzaTag;
  } else if (type_name == kProtoNames[kStreamErrorStanzaTag]) {
    return kStreamErrorStanzaTag;
  }
  return -1;
}

std::string GetPersistentId(const google::protobuf::MessageLite& protobuf) {
  if (protobuf.GetTypeName() == kProtoNames[kIqStanzaTag]) {
    return reinterpret_cast<const mcs_proto::IqStanza*>(&protobuf)->
        persistent_id();
  } else if (protobuf.GetTypeName() == kProtoNames[kDataMessageStanzaTag]) {
    return reinterpret_cast<const mcs_proto::DataMessageStanza*>(&protobuf)->
        persistent_id();
  }
  // Not all message types have persistent ids. Just return empty string;
  return "";
}

void SetPersistentId(const std::string& persistent_id,
                     google::protobuf::MessageLite* protobuf) {
  if (protobuf->GetTypeName() == kProtoNames[kIqStanzaTag]) {
    reinterpret_cast<mcs_proto::IqStanza*>(protobuf)->
        set_persistent_id(persistent_id);
    return;
  } else if (protobuf->GetTypeName() == kProtoNames[kDataMessageStanzaTag]) {
    reinterpret_cast<mcs_proto::DataMessageStanza*>(protobuf)->
        set_persistent_id(persistent_id);
    return;
  }
  NOTREACHED();
}

uint32_t GetLastStreamIdReceived(
    const google::protobuf::MessageLite& protobuf) {
  if (protobuf.GetTypeName() == kProtoNames[kIqStanzaTag]) {
    return reinterpret_cast<const mcs_proto::IqStanza*>(&protobuf)->
        last_stream_id_received();
  } else if (protobuf.GetTypeName() == kProtoNames[kDataMessageStanzaTag]) {
    return reinterpret_cast<const mcs_proto::DataMessageStanza*>(&protobuf)->
        last_stream_id_received();
  } else if (protobuf.GetTypeName() == kProtoNames[kHeartbeatPingTag]) {
    return reinterpret_cast<const mcs_proto::HeartbeatPing*>(&protobuf)->
        last_stream_id_received();
  } else if (protobuf.GetTypeName() == kProtoNames[kHeartbeatAckTag]) {
    return reinterpret_cast<const mcs_proto::HeartbeatAck*>(&protobuf)->
        last_stream_id_received();
  } else if (protobuf.GetTypeName() == kProtoNames[kLoginResponseTag]) {
    return reinterpret_cast<const mcs_proto::LoginResponse*>(&protobuf)->
        last_stream_id_received();
  }
  // Not all message types have last stream ids. Just return 0.
  return 0;
}

void SetLastStreamIdReceived(uint32_t val,
                             google::protobuf::MessageLite* protobuf) {
  if (protobuf->GetTypeName() == kProtoNames[kIqStanzaTag]) {
    reinterpret_cast<mcs_proto::IqStanza*>(protobuf)->
        set_last_stream_id_received(val);
    return;
  } else if (protobuf->GetTypeName() == kProtoNames[kHeartbeatPingTag]) {
    reinterpret_cast<mcs_proto::HeartbeatPing*>(protobuf)->
        set_last_stream_id_received(val);
    return;
  } else if (protobuf->GetTypeName() == kProtoNames[kHeartbeatAckTag]) {
    reinterpret_cast<mcs_proto::HeartbeatAck*>(protobuf)->
        set_last_stream_id_received(val);
    return;
  } else if (protobuf->GetTypeName() == kProtoNames[kDataMessageStanzaTag]) {
    reinterpret_cast<mcs_proto::DataMessageStanza*>(protobuf)->
        set_last_stream_id_received(val);
    return;
  } else if (protobuf->GetTypeName() == kProtoNames[kLoginResponseTag]) {
    reinterpret_cast<mcs_proto::LoginResponse*>(protobuf)->
        set_last_stream_id_received(val);
    return;
  }
  NOTREACHED();
}

bool HasTTLExpired(const google::protobuf::MessageLite& protobuf,
                   base::Clock* clock) {
  if (protobuf.GetTypeName() != kProtoNames[kDataMessageStanzaTag])
    return false;
  uint64_t ttl = GetTTL(protobuf);
  uint64_t sent =
      reinterpret_cast<const mcs_proto::DataMessageStanza*>(&protobuf)->sent();
  DCHECK(sent);
  return ttl > 0 &&
      clock->Now() >
          base::Time::FromInternalValue(
              (sent + ttl) * base::Time::kMicrosecondsPerSecond);
}

int GetTTL(const google::protobuf::MessageLite& protobuf) {
  if (protobuf.GetTypeName() != kProtoNames[kDataMessageStanzaTag])
    return 0;
  const mcs_proto::DataMessageStanza* data_message =
      reinterpret_cast<const mcs_proto::DataMessageStanza*>(&protobuf);
  if (!data_message->has_ttl())
    return kMaxTTLSeconds;
  DCHECK_LE(data_message->ttl(), kMaxTTLSeconds);
  return data_message->ttl();
}

bool IsImmediateAckRequested(const google::protobuf::MessageLite& protobuf) {
  if (protobuf.GetTypeName() != kProtoNames[kDataMessageStanzaTag])
    return false;
  const mcs_proto::DataMessageStanza* data_message =
      reinterpret_cast<const mcs_proto::DataMessageStanza*>(&protobuf);
  return data_message->has_immediate_ack() && data_message->immediate_ack();
}

}  // namespace gcm
