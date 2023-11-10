// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/chromoting_event.h"

#include "base/strings/string_util.h"
#include "base/strings/stringize_macros.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "remoting/base/name_value_map.h"

namespace remoting {

namespace {

const NameMapElement<ChromotingEvent::AuthMethod> kAuthMethodNames[]{
    {ChromotingEvent::AuthMethod::PIN, "pin"},
    {ChromotingEvent::AuthMethod::ACCESS_CODE, "access-code"},
    {ChromotingEvent::AuthMethod::PINLESS, "pinless"},
    {ChromotingEvent::AuthMethod::THIRD_PARTY, "third-party"},
};

const NameMapElement<ChromotingEvent::ConnectionError> kConnectionErrorNames[]{
    {ChromotingEvent::ConnectionError::NONE, "none"},
    {ChromotingEvent::ConnectionError::HOST_OFFLINE, "host-offline"},
    {ChromotingEvent::ConnectionError::SESSION_REJECTED, "session-rejected"},
    {ChromotingEvent::ConnectionError::INCOMPATIBLE_PROTOCOL,
     "incompatible-protocol"},
    {ChromotingEvent::ConnectionError::NETWORK_FAILURE, "network-failure"},
    {ChromotingEvent::ConnectionError::UNKNOWN_ERROR, "unknown-error"},
    {ChromotingEvent::ConnectionError::INVALID_ACCESS_CODE,
     "invalid-access-code"},
    {ChromotingEvent::ConnectionError::MISSING_PLUGIN, "missing-plugin"},
    {ChromotingEvent::ConnectionError::AUTHENTICATION_FAILED,
     "authentication-failed"},
    {ChromotingEvent::ConnectionError::BAD_VERSION, "bad-version"},
    {ChromotingEvent::ConnectionError::HOST_OVERLOAD, "host-overload"},
    {ChromotingEvent::ConnectionError::P2P_FAILURE, "p2p-failure"},
    {ChromotingEvent::ConnectionError::UNEXPECTED, "unexpected"},
    {ChromotingEvent::ConnectionError::CLIENT_SUSPENDED, "client-suspended"},
    {ChromotingEvent::ConnectionError::NACL_DISABLED, "nacl-disabled"},
    {ChromotingEvent::ConnectionError::MAX_SESSION_LENGTH,
     "max-session-length"},
    {ChromotingEvent::ConnectionError::HOST_CONFIGURATION_ERROR,
     "host-configuration-error"},
    {ChromotingEvent::ConnectionError::NACL_PLUGIN_CRASHED,
     "nacl-plugin-crashed"},
    {ChromotingEvent::ConnectionError::INVALID_ACCOUNT, "invalid-account"},
};

const NameMapElement<ChromotingEvent::ConnectionType> kConnectionTypeNames[]{
    {ChromotingEvent::ConnectionType::DIRECT, "direct"},
    {ChromotingEvent::ConnectionType::STUN, "stun"},
    {ChromotingEvent::ConnectionType::RELAY, "relay"},
};

const NameMapElement<ChromotingEvent::Mode> kModeNames[]{
    {ChromotingEvent::Mode::IT2ME, "it2me"},
    {ChromotingEvent::Mode::ME2ME, "me2me"},
};

const NameMapElement<ChromotingEvent::Os> kOsNames[] = {
    {ChromotingEvent::Os::CHROMOTING_LINUX, "linux"},
    {ChromotingEvent::Os::CHROMOTING_CHROMEOS, "chromeos"},
    {ChromotingEvent::Os::CHROMOTING_MAC, "mac"},
    {ChromotingEvent::Os::CHROMOTING_WINDOWS, "windows"},
    {ChromotingEvent::Os::OTHER, "other"},
    {ChromotingEvent::Os::CHROMOTING_ANDROID, "android"},
    {ChromotingEvent::Os::CHROMOTING_IOS, "ios"},
};

const NameMapElement<ChromotingEvent::SessionState> kSessionStateNames[]{
    {ChromotingEvent::SessionState::UNKNOWN, "unknown"},
    {ChromotingEvent::SessionState::CREATED, "created"},
    {ChromotingEvent::SessionState::BAD_PLUGIN_VERSION, "bad-plugin-version"},
    {ChromotingEvent::SessionState::UNKNOWN_PLUGIN_ERROR,
     "unknown-plugin-error"},
    {ChromotingEvent::SessionState::CONNECTING, "connecting"},
    {ChromotingEvent::SessionState::INITIALIZING, "initializing"},
    {ChromotingEvent::SessionState::CONNECTED, "connected"},
    {ChromotingEvent::SessionState::CLOSED, "closed"},
    {ChromotingEvent::SessionState::CONNECTION_FAILED, "connection-failed"},
    {ChromotingEvent::SessionState::UNDEFINED, "undefined"},
    {ChromotingEvent::SessionState::PLUGIN_DISABLED, "plugin-disabled"},
    {ChromotingEvent::SessionState::CONNECTION_DROPPED, "connection-dropped"},
    {ChromotingEvent::SessionState::CONNECTION_CANCELED, "connection-canceled"},
    {ChromotingEvent::SessionState::AUTHENTICATED, "authenticated"},
    {ChromotingEvent::SessionState::STARTED, "started"},
    {ChromotingEvent::SessionState::SIGNALING, "signaling"},
    {ChromotingEvent::SessionState::CREATING_PLUGIN, "creating-plugin"},
};

}  // namespace

const char ChromotingEvent::kAuthMethodKey[] = "auth_method";
const char ChromotingEvent::kCaptureLatencyKey[] = "capture_latency";
const char ChromotingEvent::kConnectionErrorKey[] = "connection_error";
const char ChromotingEvent::kConnectionTypeKey[] = "connection_type";
const char ChromotingEvent::kCpuKey[] = "cpu";
const char ChromotingEvent::kDecodeLatencyKey[] = "decode_latency";
const char ChromotingEvent::kEncodeLatencyKey[] = "encode_latency";
const char ChromotingEvent::kHostOsKey[] = "host_os";
const char ChromotingEvent::kHostOsVersionKey[] = "host_os_version";
const char ChromotingEvent::kHostVersionKey[] = "host_version";
const char ChromotingEvent::kMaxCaptureLatencyKey[] = "max_capture_latency";
const char ChromotingEvent::kMaxDecodeLatencyKey[] = "max_decode_latency";
const char ChromotingEvent::kMaxEncodeLatencyKey[] = "max_encode_latency";
const char ChromotingEvent::kMaxRenderLatencyKey[] = "max_render_latency";
const char ChromotingEvent::kMaxRoundtripLatencyKey[] = "max_roundtrip_latency";
const char ChromotingEvent::kModeKey[] = "mode";
const char ChromotingEvent::kOsKey[] = "os";
const char ChromotingEvent::kOsVersionKey[] = "os_version";
const char ChromotingEvent::kPreviousSessionStateKey[] =
    "previous_session_state";
const char ChromotingEvent::kRenderLatencyKey[] = "render_latency";
const char ChromotingEvent::kRoleKey[] = "role";
const char ChromotingEvent::kRoundtripLatencyKey[] = "roundtrip_latency";
const char ChromotingEvent::kSessionDurationKey[] = "session_duration";
const char ChromotingEvent::kSessionEntryPointKey[] = "session_entry_point";
const char ChromotingEvent::kSessionIdKey[] = "session_id";
const char ChromotingEvent::kSessionStateKey[] = "session_state";
const char ChromotingEvent::kSignalStrategyTypeKey[] = "signal_strategy_type";
const char ChromotingEvent::kTypeKey[] = "type";
const char ChromotingEvent::kVideoBandwidthKey[] = "video_bandwidth";
const char ChromotingEvent::kWebAppVersionKey[] = "webapp_version";

ChromotingEvent::ChromotingEvent()
    : values_map_(std::make_unique<base::Value::Dict>()) {}

ChromotingEvent::ChromotingEvent(Type type) : ChromotingEvent() {
  SetEnum(kTypeKey, type);
}

ChromotingEvent::ChromotingEvent(const ChromotingEvent& other) {
  send_attempts_ = other.send_attempts_;
  values_map_ = std::make_unique<base::Value::Dict>(other.values_map_->Clone());
}

ChromotingEvent::ChromotingEvent(ChromotingEvent&& other) {
  send_attempts_ = other.send_attempts_;
  values_map_ = std::move(other.values_map_);
}

ChromotingEvent::~ChromotingEvent() = default;

ChromotingEvent& ChromotingEvent::operator=(const ChromotingEvent& other) {
  if (this != &other) {
    send_attempts_ = other.send_attempts_;
    values_map_ =
        std::make_unique<base::Value::Dict>(other.values_map_->Clone());
  }
  return *this;
}

ChromotingEvent& ChromotingEvent::operator=(ChromotingEvent&& other) {
  send_attempts_ = other.send_attempts_;
  values_map_ = std::move(other.values_map_);
  return *this;
}

void ChromotingEvent::SetString(const std::string& key,
                                const std::string& value) {
  values_map_->Set(key, value);
}

void ChromotingEvent::SetInteger(const std::string& key, int value) {
  values_map_->Set(key, value);
}

void ChromotingEvent::SetBoolean(const std::string& key, bool value) {
  values_map_->Set(key, value);
}

void ChromotingEvent::SetDouble(const std::string& key, double value) {
  values_map_->Set(key, value);
}

bool ChromotingEvent::IsDataValid() {
  std::optional<int> auth_method = values_map_->FindInt(kAuthMethodKey);
  if (auth_method &&
      auth_method.value() == static_cast<int>(AuthMethod::NOT_SET)) {
    return false;
  }
  // TODO(yuweih): We may add other checks.

  return true;
}

void ChromotingEvent::AddSystemInfo() {
  SetString(kCpuKey, base::SysInfo::OperatingSystemArchitecture());
  SetString(kOsVersionKey, base::SysInfo::OperatingSystemVersion());
  SetString(kWebAppVersionKey, STRINGIZE(VERSION));
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  Os os = Os::CHROMOTING_LINUX;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  Os os = Os::CHROMOTING_CHROMEOS;
#elif BUILDFLAG(IS_IOS)
  Os os = Os::CHROMOTING_IOS;
#elif BUILDFLAG(IS_MAC)
  Os os = Os::CHROMOTING_MAC;
#elif BUILDFLAG(IS_WIN)
  Os os = Os::CHROMOTING_WINDOWS;
#elif BUILDFLAG(IS_ANDROID)
  Os os = Os::CHROMOTING_ANDROID;
#else
  Os os = Os::OTHER;
#endif
  SetEnum(kOsKey, os);
}

void ChromotingEvent::IncrementSendAttempts() {
  send_attempts_++;
}

const base::Value* ChromotingEvent::GetValue(const std::string& key) const {
  return values_map_->Find(key);
}

std::unique_ptr<base::Value::Dict> ChromotingEvent::CopyDictionaryValue()
    const {
  return std::make_unique<base::Value::Dict>(values_map_->Clone());
}

// TODO(rkjnsn): Ideally we'd use the protobuf directly instead of storing
// everything in a DictionaryValue that needs to be converted.
apis::v1::ChromotingEvent ChromotingEvent::CreateProto() const {
  apis::v1::ChromotingEvent event_proto;

  if (std::optional<int> auth_method = values_map_->FindInt(kAuthMethodKey)) {
    event_proto.set_auth_method(
        static_cast<apis::v1::ChromotingEvent_AuthMethod>(auth_method.value()));
  }
  if (std::optional<double> capture_latency =
          values_map_->FindDouble(kCaptureLatencyKey)) {
    event_proto.set_capture_latency(capture_latency.value());
  }
  if (std::optional<int> connection_error =
          values_map_->FindInt(kConnectionErrorKey)) {
    event_proto.set_connection_error(
        static_cast<apis::v1::ChromotingEvent_ConnectionError>(
            connection_error.value()));
  }
  if (std::optional<int> connection_type =
          values_map_->FindInt(kConnectionTypeKey)) {
    event_proto.set_connection_type(
        static_cast<apis::v1::ChromotingEvent_ConnectionType>(
            connection_type.value()));
  }
  if (const std::string* cpu = values_map_->FindString(kCpuKey)) {
    event_proto.set_cpu(*cpu);
  }
  if (std::optional<double> decode_latency =
          values_map_->FindDouble(kDecodeLatencyKey)) {
    event_proto.set_decode_latency(decode_latency.value());
  }
  if (std::optional<double> encode_latency =
          values_map_->FindDouble(kEncodeLatencyKey)) {
    event_proto.set_encode_latency(encode_latency.value());
  }
  if (std::optional<int> host_os = values_map_->FindInt(kHostOsKey)) {
    event_proto.set_host_os(
        static_cast<apis::v1::ChromotingEvent_Os>(host_os.value()));
  }
  if (const std::string* host_os_version =
          values_map_->FindString(kHostOsVersionKey)) {
    event_proto.set_host_os_version(*host_os_version);
  }
  if (const std::string* host_version =
          values_map_->FindString(kHostVersionKey)) {
    event_proto.set_host_version(*host_version);
  }
  if (std::optional<double> max_capture_latency =
          values_map_->FindDouble(kMaxCaptureLatencyKey)) {
    event_proto.set_max_capture_latency(max_capture_latency.value());
  }
  if (std::optional<double> max_decode_latency =
          values_map_->FindDouble(kMaxDecodeLatencyKey)) {
    event_proto.set_max_decode_latency(max_decode_latency.value());
  }
  if (std::optional<double> max_encode_latency =
          values_map_->FindDouble(kMaxEncodeLatencyKey)) {
    event_proto.set_max_encode_latency(max_encode_latency.value());
  }
  if (std::optional<double> max_render_latency =
          values_map_->FindDouble(kMaxRenderLatencyKey)) {
    event_proto.set_max_render_latency(max_render_latency.value());
  }
  if (std::optional<double> max_roundtrip_latency =
          values_map_->FindDouble(kMaxRoundtripLatencyKey)) {
    event_proto.set_max_roundtrip_latency(max_roundtrip_latency.value());
  }
  if (std::optional<int> mode = values_map_->FindInt(kModeKey)) {
    event_proto.set_mode(
        static_cast<apis::v1::ChromotingEvent_Mode>(mode.value()));
  }
  if (std::optional<int> os = values_map_->FindInt(kOsKey)) {
    event_proto.set_os(static_cast<apis::v1::ChromotingEvent_Os>(os.value()));
  }
  if (const std::string* os_version = values_map_->FindString(kOsVersionKey)) {
    event_proto.set_os_version(*os_version);
  }
  if (std::optional<int> previous_session_state =
          values_map_->FindInt(kPreviousSessionStateKey)) {
    event_proto.set_previous_session_state(
        static_cast<apis::v1::ChromotingEvent_SessionState>(
            previous_session_state.value()));
  }
  if (std::optional<double> render_latency =
          values_map_->FindDouble(kRenderLatencyKey)) {
    event_proto.set_render_latency(render_latency.value());
  }
  if (std::optional<int> role = values_map_->FindInt(kRoleKey)) {
    event_proto.set_role(
        static_cast<apis::v1::ChromotingEvent_Role>(role.value()));
  }
  if (std::optional<double> roundtrip_latency =
          values_map_->FindDouble(kRoundtripLatencyKey)) {
    event_proto.set_roundtrip_latency(roundtrip_latency.value());
  }
  if (std::optional<double> session_duration =
          values_map_->FindDouble(kSessionDurationKey)) {
    event_proto.set_session_duration(session_duration.value());
  }
  if (std::optional<int> session_entry_point =
          values_map_->FindInt(kSessionEntryPointKey)) {
    event_proto.set_session_entry_point(
        static_cast<apis::v1::ChromotingEvent_SessionEntryPoint>(
            session_entry_point.value()));
  }
  if (const std::string* session_id = values_map_->FindString(kSessionIdKey)) {
    event_proto.set_session_id(*session_id);
  }
  if (std::optional<int> session_state =
          values_map_->FindInt(kSessionStateKey)) {
    event_proto.set_session_state(
        static_cast<apis::v1::ChromotingEvent_SessionState>(
            session_state.value()));
  }
  if (std::optional<int> signal_strategy_type =
          values_map_->FindInt(kSignalStrategyTypeKey)) {
    event_proto.set_signal_strategy_type(
        static_cast<apis::v1::ChromotingEvent_SignalStrategyType>(
            signal_strategy_type.value()));
  }
  if (std::optional<int> type = values_map_->FindInt(kTypeKey)) {
    event_proto.set_type(
        static_cast<apis::v1::ChromotingEvent_Type>(type.value()));
  }
  if (std::optional<double> video_bandwidth =
          values_map_->FindDouble(kVideoBandwidthKey)) {
    event_proto.set_video_bandwidth(video_bandwidth.value());
  }
  if (const std::string* webapp_version =
          values_map_->FindString(kWebAppVersionKey)) {
    event_proto.set_webapp_version(*webapp_version);
  }
  return event_proto;
}

// static
bool ChromotingEvent::IsEndOfSession(SessionState state) {
  return state == SessionState::CLOSED ||
         state == SessionState::CONNECTION_DROPPED ||
         state == SessionState::CONNECTION_FAILED ||
         state == SessionState::CONNECTION_CANCELED;
}

// static
ChromotingEvent::Os ChromotingEvent::ParseOsFromString(const std::string& os) {
  ChromotingEvent::Os result;
  if (!NameToValue(kOsNames, base::ToLowerASCII(os), &result)) {
    return Os::OTHER;
  }

  return result;
}

// static
template <>
const char* ChromotingEvent::EnumToString(AuthMethod value) {
  return ValueToName(kAuthMethodNames, value);
}

// static
template <>
const char* ChromotingEvent::EnumToString(ConnectionError value) {
  return ValueToName(kConnectionErrorNames, value);
}

// static
template <>
const char* ChromotingEvent::EnumToString(ConnectionType value) {
  return ValueToName(kConnectionTypeNames, value);
}

// static
template <>
const char* ChromotingEvent::EnumToString(Mode value) {
  return ValueToName(kModeNames, value);
}

// static
template <>
const char* ChromotingEvent::EnumToString(Os value) {
  return ValueToName(kOsNames, value);
}

// static
template <>
const char* ChromotingEvent::EnumToString(SessionState value) {
  return ValueToName(kSessionStateNames, value);
}

}  // namespace remoting
