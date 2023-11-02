// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/heartbeat_sender.h"

#include <math.h>

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringize_macros.h"
#include "base/system/sys_info.h"
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
#include "remoting/host/host_details.h"
#include "remoting/host/server_log_entry_host.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"

// Needed for GetComputerNameExW/ComputerNameDnsFullyQualified.
#include <windows.h>
#endif

namespace remoting {

namespace {

constexpr char kHeartbeatPath[] = "/v1/directory:heartbeat";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
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

constexpr base::TimeDelta kMinimumHeartbeatInterval = base::Minutes(3);
constexpr base::TimeDelta kHeartbeatResponseTimeout = base::Seconds(30);
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

std::string GetHostname() {
// TODO(crbug.com/1052397): Revisit the macro expression once build flag
// switch of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  return net::GetHostName();
#elif BUILDFLAG(IS_WIN)
  wchar_t buffer[MAX_PATH] = {0};
  DWORD size = MAX_PATH;
  if (!::GetComputerNameExW(ComputerNameDnsFullyQualified, buffer, &size)) {
    PLOG(ERROR) << "GetComputerNameExW failed";
    return std::string();
  }
  std::string hostname;
  if (!base::WideToUTF8(buffer, size, &hostname)) {
    LOG(ERROR) << "Failed to convert from Wide to UTF8";
    return std::string();
  }
  return hostname;
#else
  return std::string();
#endif
}

}  // namespace

class HeartbeatSender::HeartbeatClientImpl final
    : public HeartbeatSender::HeartbeatClient {
 public:
  explicit HeartbeatClientImpl(
      OAuthTokenGetter* oauth_token_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  HeartbeatClientImpl(const HeartbeatClientImpl&) = delete;
  HeartbeatClientImpl& operator=(const HeartbeatClientImpl&) = delete;

  ~HeartbeatClientImpl() override;

  void Heartbeat(std::unique_ptr<apis::v1::HeartbeatRequest> request,
                 HeartbeatResponseCallback callback) override;
  void CancelPendingRequests() override;

 private:
  ProtobufHttpClient http_client_;
};

HeartbeatSender::HeartbeatClientImpl::HeartbeatClientImpl(
    OAuthTokenGetter* oauth_token_getter,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : http_client_(ServiceUrls::GetInstance()->remoting_server_endpoint(),
                   oauth_token_getter,
                   url_loader_factory) {}

HeartbeatSender::HeartbeatClientImpl::~HeartbeatClientImpl() = default;

void HeartbeatSender::HeartbeatClientImpl::Heartbeat(
    std::unique_ptr<apis::v1::HeartbeatRequest> request,
    HeartbeatResponseCallback callback) {
  std::string host_offline_reason =
      request->has_host_offline_reason()
          ? (" host_offline_reason: " + request->host_offline_reason())
          : "";
  HOST_LOG << "Sending outgoing heartbeat." << host_offline_reason;

  auto request_config =
      std::make_unique<ProtobufHttpRequestConfig>(kTrafficAnnotation);
  request_config->path = kHeartbeatPath;
  request_config->request_message = std::move(request);
  auto http_request =
      std::make_unique<ProtobufHttpRequest>(std::move(request_config));
  http_request->SetTimeoutDuration(kHeartbeatResponseTimeout);
  http_request->SetResponseCallback(std::move(callback));
  http_client_.ExecuteRequest(std::move(http_request));
}

void HeartbeatSender::HeartbeatClientImpl::CancelPendingRequests() {
  http_client_.CancelPendingRequests();
}

// end of HeartbeatSender::HeartbeatClientImpl

HeartbeatSender::HeartbeatSender(
    Delegate* delegate,
    const std::string& host_id,
    SignalStrategy* signal_strategy,
    OAuthTokenGetter* oauth_token_getter,
    Observer* observer,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    bool is_googler)
    : delegate_(delegate),
      host_id_(host_id),
      signal_strategy_(signal_strategy),
      client_(std::make_unique<HeartbeatClientImpl>(oauth_token_getter,
                                                    url_loader_factory)),
      oauth_token_getter_(oauth_token_getter),
      observer_(observer),
      backoff_(&kBackoffPolicy) {
  DCHECK(delegate_);
  DCHECK(signal_strategy_);
  DCHECK(observer_);

  signal_strategy_->AddListener(this);
  OnSignalStrategyStateChange(signal_strategy_->GetState());
  is_googler_ = is_googler;
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
    SendHeartbeat();
  }
}

void HeartbeatSender::OnSignalStrategyStateChange(SignalStrategy::State state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state) {
    case SignalStrategy::State::CONNECTED:
      SendHeartbeat();
      break;
    case SignalStrategy::State::DISCONNECTED:
      client_->CancelPendingRequests();
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

void HeartbeatSender::SendHeartbeat() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (signal_strategy_->GetState() != SignalStrategy::State::CONNECTED) {
    LOG(WARNING) << "Not sending heartbeat because the signal strategy is not "
                    "connected.";
    return;
  }

  VLOG(1) << "About to send heartbeat.";

  // Drop previous heartbeat and timer so that it doesn't interfere with the
  // current one.
  client_->CancelPendingRequests();
  heartbeat_timer_.AbandonAndStop();

  client_->Heartbeat(
      CreateHeartbeatRequest(),
      base::BindOnce(&HeartbeatSender::OnResponse, base::Unretained(this)));
  observer_->OnHeartbeatSent();
}

void HeartbeatSender::OnResponse(
    const ProtobufHttpStatus& status,
    std::unique_ptr<apis::v1::HeartbeatResponse> response) {
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
      return;
    }

    if (response->has_remote_command()) {
      LOG(WARNING) << "Remote command ignored: " << response->remote_command();
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
    return;
  }

  if (status.error_code() == ProtobufHttpStatus::Code::UNAUTHENTICATED) {
    oauth_token_getter_->InvalidateCache();
    if (backoff_.failure_count() > kMaxResendOnUnauthenticatedCount) {
      delegate_->OnAuthFailed();
      return;
    }
  }

  // Calculate delay before sending the next message.
  base::TimeDelta delay;
  switch (status.error_code()) {
    case ProtobufHttpStatus::Code::OK:
      delay = base::Seconds(response->set_interval_seconds());
      if (delay < kMinimumHeartbeatInterval) {
        LOG(WARNING) << "Received suspicious set_interval_seconds: " << delay
                     << ". Using minimum interval: "
                     << kMinimumHeartbeatInterval;
        delay = kMinimumHeartbeatInterval;
      }
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

  heartbeat_timer_.Start(FROM_HERE, delay, this,
                         &HeartbeatSender::SendHeartbeat);
}

std::unique_ptr<apis::v1::HeartbeatRequest>
HeartbeatSender::CreateHeartbeatRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto heartbeat = std::make_unique<apis::v1::HeartbeatRequest>();
  heartbeat->set_tachyon_id(signal_strategy_->GetLocalAddress().id());
  heartbeat->set_host_id(host_id_);
  if (!host_offline_reason_.empty()) {
    heartbeat->set_host_offline_reason(host_offline_reason_);
  }
  heartbeat->set_host_version(STRINGIZE(VERSION));
  heartbeat->set_host_os_name(GetHostOperatingSystemName());
  heartbeat->set_host_os_version(GetHostOperatingSystemVersion());
  heartbeat->set_host_cpu_type(base::SysInfo::OperatingSystemArchitecture());
  heartbeat->set_is_initial_heartbeat(!initial_heartbeat_sent_);

  // Only set the hostname if the user's email is @google.com.
  if (is_googler_) {
    std::string hostname = GetHostname();
    if (!hostname.empty()) {
      heartbeat->set_hostname(hostname);
    }
  }

  return heartbeat;
}

}  // namespace remoting
