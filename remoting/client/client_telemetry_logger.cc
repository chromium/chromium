// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/client/client_telemetry_logger.h"

#include <memory>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "remoting/base/telemetry_log_writer.h"

#if BUILDFLAG(IS_ANDROID)
#include <android/log.h>
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

const char kSessionIdAlphabet[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
const int kSessionIdLength = 20;
const int kMaxSessionIdAgeDays = 1;

}  // namespace

namespace remoting {

struct ClientTelemetryLogger::HostInfo {
  const std::string host_version;
  const ChromotingEvent::Os host_os;
  const std::string host_os_version;
};

ClientTelemetryLogger::ClientTelemetryLogger(
    ChromotingEventLogWriter* log_writer,
    ChromotingEvent::Mode mode,
    ChromotingEvent::SessionEntryPoint entry_point)
    : mode_(mode), entry_point_(entry_point), log_writer_(log_writer) {
  thread_checker_.DetachFromThread();
}

ClientTelemetryLogger::~ClientTelemetryLogger() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ClientTelemetryLogger::SetAuthMethod(
    ChromotingEvent::AuthMethod auth_method) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(ChromotingEvent::AuthMethod::NOT_SET, auth_method);
  auth_method_ = auth_method;
}

void ClientTelemetryLogger::SetHostInfo(const std::string& host_version,
                                        ChromotingEvent::Os host_os,
                                        const std::string& host_os_version) {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_info_ = std::make_unique<HostInfo>(
      HostInfo{host_version, host_os, host_os_version});
}

void ClientTelemetryLogger::SetTransportRoute(
    const protocol::TransportRoute& route) {
  DCHECK(thread_checker_.CalledOnValidThread());
  transport_route_ = std::make_unique<protocol::TransportRoute>(route);
}

void ClientTelemetryLogger::SetSignalStrategyType(
    ChromotingEvent::SignalStrategyType signal_strategy_type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  signal_strategy_type_ = signal_strategy_type;
}

void ClientTelemetryLogger::LogSessionStateChange(
    ChromotingEvent::SessionState state,
    ChromotingEvent::ConnectionError error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RefreshSessionIdIfOutdated();
  if (session_start_time_.is_null()) {
    session_start_time_ = base::TimeTicks::Now();
  }

  ChromotingEvent event =
      ClientTelemetryLogger::MakeSessionStateChangeEvent(state, error);

  const base::Value* previous_state =
      current_session_state_event_.GetValue(ChromotingEvent::kSessionStateKey);
  if (previous_state) {
    event.SetInteger(ChromotingEvent::kPreviousSessionStateKey,
                     previous_state->GetInt());
  }

  log_writer_->Log(event);
  current_session_state_event_ = std::move(event);

  if (ChromotingEvent::IsEndOfSession(state)) {
    session_id_.clear();
    session_start_time_ = base::TimeTicks();
  }
}

void ClientTelemetryLogger::LogStatistics(
    const protocol::PerformanceTracker& perf_tracker) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RefreshSessionIdIfOutdated();

  PrintLogStatistics(perf_tracker);

  ChromotingEvent event = MakeStatsEvent(perf_tracker);
  log_writer_->Log(event);
}

void ClientTelemetryLogger::PrintLogStatistics(
    const protocol::PerformanceTracker& perf_tracker) {
#if BUILDFLAG(IS_ANDROID)
  __android_log_print(
      ANDROID_LOG_INFO, "stats",
#else
  VLOG(0) << base::StringPrintf(
#endif  // BUILDFLAG(IS_ANDROID)
      "Bandwidth:%.0f FrameRate:%.1f;"
      " (Avg, Max) Capture:%.1f, %" PRId64 " Encode:%.1f, %" PRId64
      " Decode:%.1f, %" PRId64 " Render:%.1f, %" PRId64 " RTL:%.0f, %" PRId64,
      perf_tracker.video_bandwidth(), perf_tracker.video_frame_rate(),
      perf_tracker.video_capture_ms().Average(),
      perf_tracker.video_capture_ms().Max(),
      perf_tracker.video_encode_ms().Average(),
      perf_tracker.video_encode_ms().Max(),
      perf_tracker.video_decode_ms().Average(),
      perf_tracker.video_decode_ms().Max(),
      perf_tracker.video_paint_ms().Average(),
      perf_tracker.video_paint_ms().Max(),
      perf_tracker.round_trip_ms().Average(),
      perf_tracker.round_trip_ms().Max());
}

void ClientTelemetryLogger::SetSessionIdGenerationTimeForTest(
    base::TimeTicks gen_time) {
  session_id_generation_time_ = gen_time;
}

// static
ChromotingEvent::SessionState ClientTelemetryLogger::TranslateState(
    protocol::ConnectionToHost::State current_state,
    protocol::ConnectionToHost::State previous_state) {
  switch (current_state) {
    case protocol::ConnectionToHost::State::INITIALIZING:
      return ChromotingEvent::SessionState::INITIALIZING;
    case protocol::ConnectionToHost::State::CONNECTING:
      return ChromotingEvent::SessionState::CONNECTING;
    case protocol::ConnectionToHost::State::AUTHENTICATED:
      return ChromotingEvent::SessionState::AUTHENTICATED;
    case protocol::ConnectionToHost::State::CONNECTED:
      return ChromotingEvent::SessionState::CONNECTED;
    case protocol::ConnectionToHost::State::FAILED:
      return previous_state == protocol::ConnectionToHost::State::CONNECTED
                 ? ChromotingEvent::SessionState::CONNECTION_DROPPED
                 : ChromotingEvent::SessionState::CONNECTION_FAILED;
    case protocol::ConnectionToHost::State::CLOSED:
      return ChromotingEvent::SessionState::CLOSED;
    default:
      NOTREACHED();
  }
}

// static
ChromotingEvent::ConnectionError ClientTelemetryLogger::TranslateError(
    protocol::ErrorCode error) {
  switch (error) {
    case ErrorCode::OK:
      return ChromotingEvent::ConnectionError::NONE;
    case ErrorCode::PEER_IS_OFFLINE:
      return ChromotingEvent::ConnectionError::HOST_OFFLINE;
    case ErrorCode::SESSION_REJECTED:
      return ChromotingEvent::ConnectionError::SESSION_REJECTED;
    case ErrorCode::INCOMPATIBLE_PROTOCOL:
      return ChromotingEvent::ConnectionError::INCOMPATIBLE_PROTOCOL;
    case ErrorCode::AUTHENTICATION_FAILED:
      return ChromotingEvent::ConnectionError::AUTHENTICATION_FAILED;
    case ErrorCode::INVALID_ACCOUNT:
      return ChromotingEvent::ConnectionError::INVALID_ACCOUNT;
    case ErrorCode::CHANNEL_CONNECTION_ERROR:
      return ChromotingEvent::ConnectionError::P2P_FAILURE;
    case ErrorCode::SIGNALING_ERROR:
      return ChromotingEvent::ConnectionError::NETWORK_FAILURE;
    case ErrorCode::SIGNALING_TIMEOUT:
      return ChromotingEvent::ConnectionError::NETWORK_FAILURE;
    case ErrorCode::HOST_OVERLOAD:
      return ChromotingEvent::ConnectionError::HOST_OVERLOAD;
    case ErrorCode::MAX_SESSION_LENGTH:
      return ChromotingEvent::ConnectionError::MAX_SESSION_LENGTH;
    case ErrorCode::HOST_CONFIGURATION_ERROR:
      return ChromotingEvent::ConnectionError::HOST_CONFIGURATION_ERROR;
    case ErrorCode::UNKNOWN_ERROR:
      return ChromotingEvent::ConnectionError::UNKNOWN_ERROR;
    default:
      NOTREACHED();
  }
}

// static
ChromotingEvent::ConnectionType ClientTelemetryLogger::TranslateConnectionType(
    protocol::TransportRoute::RouteType type) {
  switch (type) {
    case protocol::TransportRoute::DIRECT:
      return ChromotingEvent::ConnectionType::DIRECT;
    case protocol::TransportRoute::STUN:
      return ChromotingEvent::ConnectionType::STUN;
    case protocol::TransportRoute::RELAY:
      return ChromotingEvent::ConnectionType::RELAY;
    default:
      NOTREACHED();
  }
}

void ClientTelemetryLogger::FillEventContext(ChromotingEvent* event) const {
  event->SetEnum(ChromotingEvent::kModeKey, mode_);
  event->SetEnum(ChromotingEvent::kRoleKey, ChromotingEvent::Role::CLIENT);
  event->SetEnum(ChromotingEvent::kSessionEntryPointKey, entry_point_);
  if (auth_method_ != ChromotingEvent::AuthMethod::NOT_SET) {
    event->SetEnum(ChromotingEvent::kAuthMethodKey, auth_method_);
  }
  if (host_info_) {
    event->SetString(ChromotingEvent::kHostVersionKey,
                     host_info_->host_version);
    event->SetEnum(ChromotingEvent::kHostOsKey, host_info_->host_os);
    event->SetString(ChromotingEvent::kHostOsVersionKey,
                     host_info_->host_os_version);
  }
  if (transport_route_) {
    ChromotingEvent::ConnectionType connection_type =
        TranslateConnectionType(transport_route_->type);
    event->SetEnum(ChromotingEvent::kConnectionTypeKey, connection_type);
  }
  event->AddSystemInfo();
  if (!session_id_.empty()) {
    event->SetString(ChromotingEvent::kSessionIdKey, session_id_);
  }
  if (!session_start_time_.is_null()) {
    int session_duration =
        (base::TimeTicks::Now() - session_start_time_).InSeconds();
    event->SetInteger(ChromotingEvent::kSessionDurationKey, session_duration);
  }
  if (signal_strategy_type_ != ChromotingEvent::SignalStrategyType::NOT_SET) {
    event->SetInteger(ChromotingEvent::kSignalStrategyTypeKey,
                      signal_strategy_type_);
  }
}

void ClientTelemetryLogger::GenerateSessionId() {
  session_id_.resize(kSessionIdLength);
  for (int i = 0; i < kSessionIdLength; i++) {
    const int alphabet_size = std::size(kSessionIdAlphabet) - 1;
    session_id_[i] = kSessionIdAlphabet[base::RandGenerator(alphabet_size)];
  }
  session_id_generation_time_ = base::TimeTicks::Now();
}

void ClientTelemetryLogger::RefreshSessionIdIfOutdated() {
  if (session_id_.empty()) {
    GenerateSessionId();
    return;
  }

  base::TimeDelta max_age = base::Days(kMaxSessionIdAgeDays);
  if (base::TimeTicks::Now() - session_id_generation_time_ > max_age) {
    // Log the old session ID.
    ChromotingEvent event = MakeSessionIdOldEvent();
    log_writer_->Log(event);

    // Generate a new session ID.
    GenerateSessionId();

    // Log the new session ID.
    ChromotingEvent new_id_event = MakeSessionIdNewEvent();
    log_writer_->Log(new_id_event);
  }
}

ChromotingEvent ClientTelemetryLogger::MakeStatsEvent(
    const protocol::PerformanceTracker& perf_tracker) {
  ChromotingEvent event(ChromotingEvent::Type::CONNECTION_STATISTICS);
  FillEventContext(&event);

  event.SetDouble(ChromotingEvent::kVideoBandwidthKey,
                  perf_tracker.video_bandwidth());
  event.SetDouble(ChromotingEvent::kCaptureLatencyKey,
                  perf_tracker.video_capture_ms().Average());
  event.SetDouble(ChromotingEvent::kEncodeLatencyKey,
                  perf_tracker.video_encode_ms().Average());
  event.SetDouble(ChromotingEvent::kDecodeLatencyKey,
                  perf_tracker.video_decode_ms().Average());
  event.SetDouble(ChromotingEvent::kRenderLatencyKey,
                  perf_tracker.video_paint_ms().Average());
  event.SetDouble(ChromotingEvent::kRoundtripLatencyKey,
                  perf_tracker.round_trip_ms().Average());
  event.SetDouble(ChromotingEvent::kMaxCaptureLatencyKey,
                  perf_tracker.video_capture_ms().Max());
  event.SetDouble(ChromotingEvent::kMaxEncodeLatencyKey,
                  perf_tracker.video_encode_ms().Max());
  event.SetDouble(ChromotingEvent::kMaxDecodeLatencyKey,
                  perf_tracker.video_decode_ms().Max());
  event.SetDouble(ChromotingEvent::kMaxRenderLatencyKey,
                  perf_tracker.video_paint_ms().Max());
  event.SetDouble(ChromotingEvent::kMaxRoundtripLatencyKey,
                  perf_tracker.round_trip_ms().Max());

  return event;
}

ChromotingEvent ClientTelemetryLogger::MakeSessionStateChangeEvent(
    ChromotingEvent::SessionState state,
    ChromotingEvent::ConnectionError error) {
  ChromotingEvent event(ChromotingEvent::Type::SESSION_STATE);
  FillEventContext(&event);
  event.SetEnum(ChromotingEvent::kSessionStateKey, state);
  event.SetEnum(ChromotingEvent::kConnectionErrorKey, error);
  return event;
}

ChromotingEvent ClientTelemetryLogger::MakeSessionIdOldEvent() {
  ChromotingEvent event(ChromotingEvent::Type::SESSION_ID_OLD);
  FillEventContext(&event);
  return event;
}

ChromotingEvent ClientTelemetryLogger::MakeSessionIdNewEvent() {
  ChromotingEvent event(ChromotingEvent::Type::SESSION_ID_NEW);
  FillEventContext(&event);
  return event;
}

}  // namespace remoting
