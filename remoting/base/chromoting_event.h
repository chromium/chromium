// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CHROMOTING_EVENT_H_
#define REMOTING_BASE_CHROMOTING_EVENT_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "remoting/proto/remoting/v1/chromoting_event.pb.h"

namespace remoting {

// TODO(yuweih): See if we can rewrite this in Protobuf.
// This is the representation of the log entry being sent to the telemetry
// server. The content should be synced with chromoting_event.js and
// chromoting_extensions.proto.
class ChromotingEvent {
 public:
  enum class AuthMethod {
    // Note that NOT_SET is only defined locally and should not be sent to the
    // server.
    NOT_SET = 0,
    PIN = 1,
    ACCESS_CODE = 2,
    PINLESS = 3,
    THIRD_PARTY = 4,
  };

  enum class ConnectionError {
    NONE = 1,
    HOST_OFFLINE = 2,
    SESSION_REJECTED = 3,
    INCOMPATIBLE_PROTOCOL = 4,
    NETWORK_FAILURE = 5,
    UNKNOWN_ERROR = 6,
    INVALID_ACCESS_CODE = 7,
    MISSING_PLUGIN = 8,
    AUTHENTICATION_FAILED = 9,
    BAD_VERSION = 10,
    HOST_OVERLOAD = 11,
    P2P_FAILURE = 12,
    UNEXPECTED = 13,
    CLIENT_SUSPENDED = 14,
    NACL_DISABLED = 15,
    MAX_SESSION_LENGTH = 16,
    HOST_CONFIGURATION_ERROR = 17,
    NACL_PLUGIN_CRASHED = 18,
    INVALID_ACCOUNT = 19
  };

  enum class ConnectionType { DIRECT = 1, STUN = 2, RELAY = 3 };

  enum class Mode { IT2ME = 1, ME2ME = 2 };

  // macro defines from command line have polluted OS names like
  // "LINUX", "ANDROID", etc.
  enum class Os {
    CHROMOTING_LINUX = 1,
    CHROMOTING_CHROMEOS = 2,
    CHROMOTING_MAC = 3,
    CHROMOTING_WINDOWS = 4,
    OTHER = 5,
    CHROMOTING_ANDROID = 6,
    CHROMOTING_IOS = 7
  };

  enum class Role { CLIENT = 0, HOST = 1 };

  enum class SessionState {
    UNKNOWN = 1,               // deprecated.
    CREATED = 2,               // deprecated.
    BAD_PLUGIN_VERSION = 3,    // deprecated.
    UNKNOWN_PLUGIN_ERROR = 4,  // deprecated.
    CONNECTING = 5,
    INITIALIZING = 6,  // deprecated.
    CONNECTED = 7,
    CLOSED = 8,
    CONNECTION_FAILED = 9,
    UNDEFINED = 10,
    PLUGIN_DISABLED = 11,  // deprecated.
    CONNECTION_DROPPED = 12,
    CONNECTION_CANCELED = 13,
    AUTHENTICATED = 14,
    STARTED = 15,
    SIGNALING = 16,
    CREATING_PLUGIN = 17,
  };

  enum SessionEntryPoint {
    CONNECT_BUTTON = 1,
    RECONNECT_BUTTON = 2,
    AUTO_RECONNECT_ON_CONNECTION_DROPPED = 3,
    AUTO_RECONNECT_ON_HOST_OFFLINE = 4,
  };

  enum SignalStrategyType {
    NOT_SET = 0,
    XMPP = 1,
    WCS = 2,
    LCS = 3,
    FTL = 4,
  };

  enum class Type {
    SESSION_STATE = 1,
    CONNECTION_STATISTICS = 2,
    SESSION_ID_OLD = 3,
    SESSION_ID_NEW = 4,
    HEARTBEAT = 5,
    HEARTBEAT_REJECTED = 6,
    RESTART = 7,
    HOST_STATUS = 8,
    SIGNAL_STRATEGY_PROGRESS = 9
  };

  static const char kAuthMethodKey[];
  static const char kCaptureLatencyKey[];
  static const char kConnectionErrorKey[];
  static const char kConnectionTypeKey[];
  static const char kCpuKey[];
  static const char kDecodeLatencyKey[];
  static const char kEncodeLatencyKey[];
  static const char kHostOsKey[];
  static const char kHostOsVersionKey[];
  static const char kHostVersionKey[];
  static const char kMaxCaptureLatencyKey[];
  static const char kMaxDecodeLatencyKey[];
  static const char kMaxEncodeLatencyKey[];
  static const char kMaxRenderLatencyKey[];
  static const char kMaxRoundtripLatencyKey[];
  static const char kModeKey[];
  static const char kOsKey[];
  static const char kOsVersionKey[];
  static const char kPreviousSessionStateKey[];
  static const char kRenderLatencyKey[];
  static const char kRoleKey[];
  static const char kRoundtripLatencyKey[];
  static const char kSessionDurationKey[];
  static const char kSessionEntryPointKey[];
  static const char kSessionIdKey[];
  static const char kSessionStateKey[];
  static const char kSignalStrategyTypeKey[];
  static const char kTypeKey[];
  static const char kVideoBandwidthKey[];
  static const char kWebAppVersionKey[];

  ChromotingEvent();
  explicit ChromotingEvent(Type type);
  ChromotingEvent(const ChromotingEvent& other);
  ChromotingEvent(ChromotingEvent&& other);

  ~ChromotingEvent();

  ChromotingEvent& operator=(const ChromotingEvent& other);
  ChromotingEvent& operator=(ChromotingEvent&& other);

  // Sets an arbitrary key/value entry.
  void SetString(const std::string& key, const std::string& value);
  void SetInteger(const std::string& key, int value);
  void SetBoolean(const std::string& key, bool value);
  void SetDouble(const std::string& key, double value);
  template <typename EnumType>
  void SetEnum(const std::string& key, EnumType value) {
    SetInteger(key, static_cast<int>(value));
  }

  // Returns true if the format of ChromotingEvent can be accepted by the
  // telemetry server.
  bool IsDataValid();

  // Adds fields of CPU type, OS type and OS version.
  void AddSystemInfo();

  void IncrementSendAttempts();
  int send_attempts() const { return send_attempts_; }

  const base::Value* GetValue(const std::string& key) const;

  // Returns a copy of the internal dictionary value.
  std::unique_ptr<base::Value::Dict> CopyDictionaryValue() const;

  // Converts into a ChromotingEvent protobuf.
  apis::v1::ChromotingEvent CreateProto() const;

  // Returns true if the SessionState concludes the end of session.
  static bool IsEndOfSession(SessionState state);

  // Converts the OS type String into the enum value.
  static Os ParseOsFromString(const std::string& os);

  // Converts an enum value of a enum class defined above to its name as a
  // string, e.g. HOST_OFFLINE -> "host-offline".
  template <typename EnumType>
  static const char* EnumToString(EnumType value);

 private:
  std::unique_ptr<base::Value::Dict> values_map_;

  int send_attempts_ = 0;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CHROMOTING_EVENT_H_
