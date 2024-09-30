// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <math.h>

#include <cstdint>
#include <utility>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/base/network_interfaces.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/base/protobuf_http_client.h"
#include "remoting/base/protobuf_http_request.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/protobuf_http_status.h"
#include "remoting/base/service_urls.h"
#include "remoting/host/host_config.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace remoting {

namespace {

// TODO garykac: Remove this unused traffic annotation in a separate cl.
constexpr net::NetworkTrafficAnnotationTag kUnusedTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("heartbeat_sender",
                                        R"(
        semantics {
          sender: "Chrome Remote Desktop"
          description:
            "Sends heartbeat data to the Chrome Remote Desktop backend so that "
            "the client knows about the presence of the host."
          trigger:
            "Starting a Chrome Remote Desktop host."
          data:
            "Chrome Remote Desktop Host ID and some non-PII information about "
            "the host system such as the Chrome Remote Desktop version and the "
            "OS version."
          destination: OTHER
          destination_other: "Chrome Remote Desktop directory service"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be stopped in settings, but will not be sent "
            "if the user does not use Chrome Remote Desktop."
          policy_exception_justification:
            "Not implemented."
        })");

void IgnoreTrafficAnnotation(
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {}

constexpr base::TimeDelta kMinimumHeartbeatInterval = base::Minutes(3);
constexpr base::TimeDelta kResendDelayOnHostNotFound = base::Seconds(10);
constexpr base::TimeDelta kResendDelayOnUnauthenticated = base::Seconds(10);

constexpr int kMaxResendOnHostNotFoundCount =
    12;  // 2 minutes (12 x 10 seconds).
constexpr int kMaxResendOnUnauthenticatedCount =
    6;  // 1 minute (10 x 6 seconds).

const net::BackoffEntry::Policy kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    0,

    // Initial delay for exponential back-off in ms. (10s)
    10000,

    // Factor by which the waiting time will be multiplied.
    2,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.5,

    // Maximum amount of time we are willing to delay our request in ms. (10m)
    600000,

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    // Starts with initial delay.
    false,
};

}  // namespace

HeartbeatSender::HeartbeatSender(
    Delegate* delegate,
    const std::string& host_id,
    SignalStrategy* signal_strategy,
    OAuthTokenGetter* oauth_token_getter,
    std::unique_ptr<HeartbeatServiceClient> heartbeat_service,
    Observer* observer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool set_fqdn)
    : delegate_(delegate),
      host_id_(host_id),
      signal_strategy_(signal_strategy),
      oauth_token_getter_(oauth_token_getter),
      service_client_(std::move(heartbeat_service)),
      observer_(observer),
      backoff_(&kBackoffPolicy) {
  DCHECK(delegate_);
  DCHECK(signal_strategy_);
  DCHECK(observer_);

  signal_strategy_->AddListener(this);
  OnSignalStrategyStateChange(signal_strategy_->GetState());
  set_fqdn_ = set_fqdn;

  // Touch the unused traffic annotation value to avoid complaints.
  IgnoreTrafficAnnotation(kUnusedTrafficAnnotation);
}

HeartbeatSender::~HeartbeatSender() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  signal_strategy_->RemoveListener(this);
}

void HeartbeatSender::SetHostOfflineReason(
    const std::string& host_offline_reason,
    const base::TimeDelta& timeout,
    base::OnceCallback<void(bool success)> ack_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!host_offline_reason_ack_callback_);

  host_offline_reason_ = host_offline_reason;
  host_offline_reason_ack_callback_ = std::move(ack_callback);
  host_offline_reason_timeout_timer_.Start(
      FROM_HERE, timeout, this, &HeartbeatSender::OnHostOfflineReasonTimeout);
  if (signal_strategy_->GetState() == SignalStrategy::State::CONNECTED) {
    SendFullHeartbeat();
  }
}

void HeartbeatSender::OnSignalStrategyStateChange(SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state) {
    case SignalStrategy::State::CONNECTED:
      SendFullHeartbeat();
      break;
    case SignalStrategy::State::DISCONNECTED:
      service_client_->CancelPendingRequests();
      heartbeat_timer_.AbandonAndStop();
      break;
    default:
      // Do nothing
      break;
  }
}

bool HeartbeatSender::OnSignalStrategyIncomingStanza(
    const jingle_xmpp::XmlElement* stanza) {
  return false;
}

void HeartbeatSender::OnHostOfflineReasonTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host_offline_reason_ack_callback_);

  std::move(host_offline_reason_ack_callback_).Run(false);
}

void HeartbeatSender::OnHostOfflineReasonAck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!host_offline_reason_ack_callback_) {
    DCHECK(!host_offline_reason_timeout_timer_.IsRunning());
    return;
  }

  DCHECK(host_offline_reason_timeout_timer_.IsRunning());
  host_offline_reason_timeout_timer_.AbandonAndStop();

  std::move(host_offline_reason_ack_callback_).Run(true);
}

void HeartbeatSender::ClearHeartbeatTimer() {
  // Drop previous heartbeat and timer so that it doesn't interfere with the
  // current one.
  service_client_->CancelPendingRequests();
  heartbeat_timer_.AbandonAndStop();
}

void HeartbeatSender::SendFullHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (signal_strategy_->GetState() != SignalStrategy::State::CONNECTED) {
    LOG(WARNING) << "Not sending heartbeat because the signal strategy is not "
                    "connected.";
    return;
  }

  HOST_LOG << "Sending full heartbeat.";

  ClearHeartbeatTimer();

  std::optional<std::string> offline_reason;
  if (!host_offline_reason_.empty()) {
    offline_reason = host_offline_reason_;
    HOST_LOG << "Sending offline reason: " << host_offline_reason_;
  }
  std::optional<std::string> signaling_id;
  auto signaling_id_str = signal_strategy_->GetLocalAddress().id();
  if (!signaling_id_str.empty()) {
    signaling_id = signaling_id_str;
  }
  service_client_->SendFullHeartbeat(
      !initial_heartbeat_sent_, std::move(signaling_id),
      std::move(offline_reason),
      base::BindOnce(&HeartbeatSender::OnLegacyHeartbeatResponse,
                     base::Unretained(this)));

  observer_->OnHeartbeatSent();
}

void HeartbeatSender::SendLiteHeartbeat(bool useLiteHeartbeat) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (signal_strategy_->GetState() != SignalStrategy::State::CONNECTED) {
    LOG(WARNING) << "Not sending heartbeat because the signal strategy is not "
                    "connected.";
    return;
  }

  HOST_LOG << "Sending heartbeat.";

  ClearHeartbeatTimer();

  if (useLiteHeartbeat) {
    service_client_->SendLiteHeartbeat(base::BindOnce(
        &HeartbeatSender::OnSendHeartbeatResponse, base::Unretained(this)));
  } else {
    std::optional<std::string> offline_reason;
    if (!host_offline_reason_.empty()) {
      offline_reason = host_offline_reason_;
    }
    std::optional<std::string> signaling_id;
    auto signaling_id_str = signal_strategy_->GetLocalAddress().id();
    if (!signaling_id_str.empty()) {
      signaling_id = signaling_id_str;
    }
    service_client_->SendFullHeartbeat(
        !initial_heartbeat_sent_, std::move(signaling_id),
        std::move(offline_reason),
        base::BindOnce(&HeartbeatSender::OnLegacyHeartbeatResponse,
                       base::Unretained(this)));
  }
  observer_->OnHeartbeatSent();
}

bool HeartbeatSender::CheckHttpStatus(const ProtobufHttpStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status.ok()) {
    backoff_.Reset();

    // Notify listener of the first successful heartbeat.
    if (!initial_heartbeat_sent_) {
      delegate_->OnFirstHeartbeatSuccessful();
      initial_heartbeat_sent_ = true;
    }

    // Notify caller of SetHostOfflineReason that we got an ack and don't
    // schedule another heartbeat.
    if (!host_offline_reason_.empty()) {
      OnHostOfflineReasonAck();
      return false;
    }
  } else {
    LOG(ERROR) << "Heartbeat failed. Error code: "
               << static_cast<int>(status.error_code()) << ", "
               << status.error_message();
    backoff_.InformOfRequest(false);
  }

  if (status.error_code() == ProtobufHttpStatus::Code::DEADLINE_EXCEEDED) {
    LOG(ERROR) << "Heartbeat timed out.";
  }

  // If the host was registered immediately before it sends a heartbeat,
  // then server-side latency may prevent the server recognizing the
  // host ID in the heartbeat. So even if all of the first few heartbeats
  // get a "host ID not found" error, that's not a good enough reason to
  // exit.
  if (status.error_code() == ProtobufHttpStatus::Code::NOT_FOUND &&
      (initial_heartbeat_sent_ ||
       (backoff_.failure_count() > kMaxResendOnHostNotFoundCount))) {
    delegate_->OnHostNotFound();
    return false;
  }

  if (status.error_code() == ProtobufHttpStatus::Code::UNAUTHENTICATED) {
    oauth_token_getter_->InvalidateCache();
    if (backoff_.failure_count() > kMaxResendOnUnauthenticatedCount) {
      delegate_->OnAuthFailed();
      return false;
    }
  }

  return true;
}

base::TimeDelta HeartbeatSender::CalculateDelay(
    const ProtobufHttpStatus& status,
    std::optional<base::TimeDelta> optMinDelay) {
  // Calculate delay before sending the next message.
  base::TimeDelta delay;
  switch (status.error_code()) {
    case ProtobufHttpStatus::Code::OK:
      if (optMinDelay.has_value()) {
        LOG_IF(WARNING, *optMinDelay < kMinimumHeartbeatInterval)
            << "Received suspicious interval_seconds: " << *optMinDelay
            << ". Using minimum interval: " << kMinimumHeartbeatInterval;
      }
      delay = optMinDelay.value_or(kMinimumHeartbeatInterval);
      break;
    case ProtobufHttpStatus::Code::NOT_FOUND:
      delay = kResendDelayOnHostNotFound;
      break;
    case ProtobufHttpStatus::Code::UNAUTHENTICATED:
      delay = kResendDelayOnUnauthenticated;
      break;
    default:
      delay = backoff_.GetTimeUntilRelease();
      LOG(ERROR) << "Heartbeat failed due to unexpected error. Will retry in "
                 << delay;
      break;
  }
  return delay;
}

void HeartbeatSender::OnLegacyHeartbeatResponse(
    const ProtobufHttpStatus& status,
    std::optional<base::TimeDelta> wait_interval,
    const std::string& primary_user_email,
    std::optional<bool> require_session_authorization,
    std::optional<bool> use_lite_heartbeat) {
  if (CheckHttpStatus(status)) {
    bool useLiteHeartbeat = false;
    if (status.error_code() == ProtobufHttpStatus::Code::OK) {
      if (use_lite_heartbeat.has_value()) {
        useLiteHeartbeat = *use_lite_heartbeat;
      }
      if (!primary_user_email.empty()) {
        delegate_->OnUpdateHostOwner(primary_user_email);
      }
      if (require_session_authorization.has_value()) {
        bool require = *require_session_authorization;
        delegate_->OnUpdateRequireSessionAuthorization(require);
      }
    }
    heartbeat_timer_.Start(
        FROM_HERE, CalculateDelay(status, std::move(wait_interval)),
        base::BindOnce(&HeartbeatSender::SendLiteHeartbeat,
                       base::Unretained(this), useLiteHeartbeat));
  }
}

void HeartbeatSender::OnSendHeartbeatResponse(
    const ProtobufHttpStatus& status,
    std::optional<base::TimeDelta> wait_interval,
    const std::string& primary_user_email,
    std::optional<bool> require_session_authorization,
    std::optional<bool> use_lite_heartbeat) {
  if (CheckHttpStatus(status)) {
    heartbeat_timer_.Start(FROM_HERE,
                           CalculateDelay(status, std::move(wait_interval)),
                           base::BindOnce(&HeartbeatSender::SendLiteHeartbeat,
                                          base::Unretained(this), true));
  }
}

}  // namespace remoting
