// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/connection_factory_impl.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "google_apis/gcm/base/gcm_features.h"
#include "google_apis/gcm/engine/connection_handler_impl.h"
#include "google_apis/gcm/monitoring/gcm_stats_recorder.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_request_headers.h"
#include "net/http/proxy_fallback.h"
#include "net/log/net_log_source_type.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/ssl/ssl_config_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace gcm {

namespace {

// The amount of time a Socket read should wait before timing out.
const int kReadTimeoutMs = 30000;  // 30 seconds.

// If a connection is reset after succeeding within this window of time,
// the previous backoff entry is restored (and the connection success is treated
// as if it was transient).
const int kConnectionResetWindowSecs = 10;  // 10 seconds.

// Decides whether the last login was within kConnectionResetWindowSecs of now
// or not.
bool ShouldRestorePreviousBackoff(const base::TimeTicks& login_time,
                                  const base::TimeTicks& now_ticks) {
  return !login_time.is_null() &&
         now_ticks - login_time <= base::Seconds(kConnectionResetWindowSecs);
}

}  // namespace

ConnectionFactoryImpl::ConnectionFactoryImpl(
    const std::vector<GURL>& mcs_endpoints,
    const net::BackoffEntry::Policy& backoff_policy,
    GetProxyResolvingFactoryCallback get_socket_factory_callback,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    GCMStatsRecorder* recorder,
    network::NetworkConnectionTracker* network_connection_tracker)
    : mcs_endpoints_(mcs_endpoints),
      next_endpoint_(0),
      last_successful_endpoint_(0),
      backoff_policy_(backoff_policy),
      get_socket_factory_callback_(get_socket_factory_callback),
      connecting_(false),
      waiting_for_backoff_(false),
      waiting_for_network_online_(false),
      handshake_in_progress_(false),
      io_task_runner_(std::move(io_task_runner)),
      recorder_(recorder),
      network_connection_tracker_(network_connection_tracker),
      listener_(nullptr) {
  DCHECK_GE(mcs_endpoints_.size(), 1U);
  DCHECK(io_task_runner_);
}

ConnectionFactoryImpl::~ConnectionFactoryImpl() {
  CloseSocket();
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void ConnectionFactoryImpl::Initialize(
    const BuildLoginRequestCallback& request_builder,
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback) {
  DCHECK(!connection_handler_);
  DCHECK(read_callback_.is_null());
  DCHECK(write_callback_.is_null());

  previous_backoff_ = CreateBackoffEntry(&backoff_policy_);
  backoff_entry_ = CreateBackoffEntry(&backoff_policy_);
  request_builder_ = request_builder;
  read_callback_ = read_callback;
  write_callback_ = write_callback;

  network_connection_tracker_->AddNetworkConnectionObserver(this);
  auto type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  if (network_connection_tracker_->GetConnectionType(
          &type, base::BindOnce(&ConnectionFactoryImpl::OnConnectionChanged,
                                weak_ptr_factory_.GetWeakPtr()))) {
    waiting_for_network_online_ =
        type == network::mojom::ConnectionType::CONNECTION_NONE;
  }
}

ConnectionHandler* ConnectionFactoryImpl::GetConnectionHandler() const {
  return connection_handler_.get();
}

void ConnectionFactoryImpl::Connect() {
  if (!connection_handler_) {
    connection_handler_ = CreateConnectionHandler(
        base::Milliseconds(kReadTimeoutMs), read_callback_, write_callback_,
        base::BindRepeating(&ConnectionFactoryImpl::ConnectionHandlerCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  if (connecting_ || waiting_for_backoff_) {
    return;  // Connection attempt already in progress or pending.
  }

  if (IsEndpointReachable()) {
    return;  // Already connected.
  }

  ConnectWithBackoff();
}

ConnectionEventTracker* ConnectionFactoryImpl::GetEventTrackerForTesting() {
  return &event_tracker_;
}

void ConnectionFactoryImpl::ConnectWithBackoff() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  // If a canary managed to connect while a backoff expiration was pending,
  // just cleanup the internal state.
  if (connecting_ || handshake_in_progress_ || IsEndpointReachable()) {
    waiting_for_backoff_ = false;
    return;
  }

  if (backoff_entry_->ShouldRejectRequest()) {
    DVLOG(1) << "Delaying MCS endpoint connection for "
             << backoff_entry_->GetTimeUntilRelease().InMilliseconds()
             << " milliseconds.";
    waiting_for_backoff_ = true;
    recorder_->RecordConnectionDelayedDueToBackoff(
        backoff_entry_->GetTimeUntilRelease().InMilliseconds());
    io_task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ConnectionFactoryImpl::ConnectWithBackoff,
                       weak_ptr_factory_.GetWeakPtr()),
        backoff_entry_->GetTimeUntilRelease());
    return;
  }

  DVLOG(1) << "Attempting connection to MCS endpoint.";
  waiting_for_backoff_ = false;
  // It's necessary to close the socket before attempting any new connection,
  // otherwise it's possible to hit a use-after-free in the connection handler.
  // crbug.com/462319
  CloseSocket();
  if (!waiting_for_network_online_ ||
      !base::FeatureList::IsEnabled(
          gcm::features::kGCMAvoidConnectionWhenNetworkUnavailable)) {
    ConnectImpl(/*ignore_connection_failure=*/false);
  }
}

bool ConnectionFactoryImpl::IsEndpointReachable() const {
  return connection_handler_ && connection_handler_->CanSendMessage();
}

std::string ConnectionFactoryImpl::GetConnectionStateString() const {
  if (IsEndpointReachable()) {
    return "CONNECTED";
  }
  if (handshake_in_progress_) {
    return "HANDSHAKE IN PROGRESS";
  }
  if (connecting_) {
    return "CONNECTING";
  }
  if (waiting_for_backoff_) {
    return "WAITING FOR BACKOFF";
  }
  if (waiting_for_network_online_) {
    return "WAITING FOR NETWORK CHANGE";
  }
  return "NOT CONNECTED";
}

void ConnectionFactoryImpl::SignalConnectionReset(
    ConnectionResetReason reason) {
  if (!connection_handler_) {
    // No initial connection has been made. No need to do anything.
    return;
  }

  // A failure can trigger multiple resets, so no need to do anything if a
  // connection is already in progress.
  if (connecting_) {
    DVLOG(1) << "Connection in progress, ignoring reset.";
    return;
  }

  if (listener_) {
    DVLOG(1) << "Notifying listener of disconnect due to connection reset.";
    listener_->OnDisconnected();
  }

  recorder_->RecordConnectionResetSignaled(reason);

  // SignalConnectionReset can be called at any time without regard to whether
  // a connection attempt is currently in progress. Only notify the event
  // tracker if there is an event in progress.
  if (event_tracker_.IsEventInProgress()) {
    if (reason == LOGIN_FAILURE) {
      event_tracker_.ConnectionLoginFailed();
    }
    event_tracker_.EndConnectionAttempt();
  }

  CloseSocket();
  DCHECK(!IsEndpointReachable());

  if (waiting_for_network_online_ &&
      base::FeatureList::IsEnabled(
          gcm::features::kGCMAvoidConnectionWhenNetworkUnavailable)) {
    // Do nothing when there is no network connection.
    return;
  }

  // Network changes get special treatment as they can trigger a one-off canary
  // request that bypasses backoff (but does nothing if a connection is in
  // progress).
  if (reason == NETWORK_CHANGE) {
    // Canary attempts bypass backoff without resetting it. These will have no
    // effect if we're already in the process of connecting. Connection attempt
    // failure also does not affect backoff delay to avoid piling up delays
    // within a short period in case of many network changes.
    ConnectImpl(/*ignore_connection_failure=*/true);
    return;
  }

  if (waiting_for_backoff_) {
    // Other connection reset events can be ignored as a connection is already
    // awaiting backoff expiration.
    DVLOG(1) << "Backoff expiration pending, ignoring reset.";
    return;
  }

  if (reason == LOGIN_FAILURE ||
      ShouldRestorePreviousBackoff(last_login_time_, NowTicks())) {
    // Failures due to login, or within the reset window of a login, restore
    // the backoff entry that was saved off at login completion time.
    backoff_entry_.swap(previous_backoff_);
    backoff_entry_->InformOfRequest(false);
  }

  // At this point the last login time has been consumed or deemed irrelevant,
  // reset it.
  last_login_time_ = base::TimeTicks();

  Connect();
}

void ConnectionFactoryImpl::SetConnectionListener(
    ConnectionListener* listener) {
  listener_ = listener;
}

base::TimeTicks ConnectionFactoryImpl::NextRetryAttempt() const {
  if (!backoff_entry_) {
    return base::TimeTicks();
  }
  return backoff_entry_->GetReleaseTime();
}

void ConnectionFactoryImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type == network::mojom::ConnectionType::CONNECTION_NONE) {
    DVLOG(1) << "Network lost, resettion connection.";
    waiting_for_network_online_ = true;

    // Will only close the socket due to no network connection.
    SignalConnectionReset(NETWORK_CHANGE);
    return;
  }

  DVLOG(1) << "Connection type changed to " << type << ", reconnecting.";
  waiting_for_network_online_ = false;
  SignalConnectionReset(NETWORK_CHANGE);
}

GURL ConnectionFactoryImpl::GetCurrentEndpoint() const {
  // Note that IsEndpointReachable() returns false anytime connecting_ is true,
  // so while connecting this always uses |next_endpoint_|.
  if (IsEndpointReachable()) {
    return mcs_endpoints_[last_successful_endpoint_];
  }
  return mcs_endpoints_[next_endpoint_];
}

void ConnectionFactoryImpl::ConnectImpl(bool ignore_connection_failure) {
  event_tracker_.StartConnectionAttempt();
  StartConnection(ignore_connection_failure);
}

void ConnectionFactoryImpl::StartConnection(bool ignore_connection_failure) {
  DCHECK(!IsEndpointReachable());
  CHECK(!socket_);
  CHECK(!waiting_for_network_online_ ||
        !base::FeatureList::IsEnabled(
            gcm::features::kGCMAvoidConnectionWhenNetworkUnavailable));

  connecting_ = true;
  GURL current_endpoint = GetCurrentEndpoint();
  recorder_->RecordConnectionInitiated(current_endpoint.host());

  socket_factory_.reset();
  get_socket_factory_callback_.Run(
      socket_factory_.BindNewPipeAndPassReceiver());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gcm_connection_factory", R"(
        semantics {
          sender: "GCM Connection Factory"
          description:
            "TCP connection to the Google Cloud Messaging notification "
            "servers. Supports reliable bi-directional messaging and push "
            "notifications for multiple consumers."
          trigger:
            "The connection is created when an application (e.g. Chrome Sync) "
            "or a website using Web Push starts the GCM service, and is kept "
            "alive as long as there are valid applications registered. "
            "Messaging is application/website controlled."
          data:
            "Arbitrary application-specific data."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can stop messages related to Sync by disabling Sync for "
            "everything in settings. Messages related to Web Push can be "
            "stopped by revoking the site permissions in settings. Messages "
            "related to extensions can be stopped by uninstalling the "
            "extension."
          chrome_policy {
            SyncDisabled {
              SyncDisabled: True
            }
          }
        }
        comments:
          "'SyncDisabled' policy disables messages that are based on Sync, "
          "but does not have any effect on other Google Cloud messages."
        )");

  network::mojom::ProxyResolvingSocketOptionsPtr options =
      network::mojom::ProxyResolvingSocketOptions::New();
  options->use_tls = true;
  // |current_endpoint| is always a Google URL, so this NetworkAnonymizationKey
  // will be the same for all callers, and will allow pooling all connections to
  // GCM in one socket connection, if an H2 or QUIC proxy is in use.
  auto site = net::SchemefulSite(current_endpoint);
  socket_factory_->CreateProxyResolvingSocket(
      current_endpoint, net::NetworkAnonymizationKey::CreateSameSite(site),
      std::move(options),
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation),
      socket_.BindNewPipeAndPassReceiver(), mojo::NullRemote() /* observer */,
      base::BindOnce(&ConnectionFactoryImpl::OnConnectDone,
                     base::Unretained(this), ignore_connection_failure));
}

void ConnectionFactoryImpl::InitHandler(
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  mcs_proto::LoginRequest login_request;
  // May be null in tests.
  if (!request_builder_.is_null()) {
    request_builder_.Run(&login_request);
    DCHECK(login_request.IsInitialized());
    event_tracker_.WriteToLoginRequest(&login_request);
  }

  connection_handler_->Init(login_request, std::move(receive_stream),
                            std::move(send_stream));
}

std::unique_ptr<net::BackoffEntry> ConnectionFactoryImpl::CreateBackoffEntry(
    const net::BackoffEntry::Policy* const policy) {
  return std::make_unique<net::BackoffEntry>(policy);
}

std::unique_ptr<ConnectionHandler>
ConnectionFactoryImpl::CreateConnectionHandler(
    base::TimeDelta read_timeout,
    const ConnectionHandler::ProtoReceivedCallback& read_callback,
    const ConnectionHandler::ProtoSentCallback& write_callback,
    const ConnectionHandler::ConnectionChangedCallback& connection_callback) {
  return base::WrapUnique<ConnectionHandler>(
      new ConnectionHandlerImpl(io_task_runner_, read_timeout, read_callback,
                                write_callback, connection_callback));
}

base::TimeTicks ConnectionFactoryImpl::NowTicks() {
  return base::TimeTicks::Now();
}

void ConnectionFactoryImpl::OnConnectDone(
    bool ignore_connection_failure,
    int result,
    const std::optional<net::IPEndPoint>& local_addr,
    const std::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  base::UmaHistogramSparse("GCM.MCSClientConnectionNetworkCode", -result);
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (!connection_handler_) {
    // If CloseSocket() is called while a connect is pending, this callback will
    // be called with net::ERR_ABORTED. Checking |connection_handler_| serves as
    // a proxy to checking whether CloseSocket() is called.
    DCHECK_EQ(net::ERR_ABORTED, result);
    return;
  }
  if (result != net::OK) {
    LOG(ERROR) << "Failed to connect to MCS endpoint with error " << result;
    recorder_->RecordConnectionFailure(result);
    CloseSocket();
    if (!ignore_connection_failure ||
        !base::FeatureList::IsEnabled(
            gcm::features::kGCMDoNotIncreaseBackoffDelayOnNetworkChange)) {
      // Do not inform of a failed request when `ignore_connection_failure`.
      backoff_entry_->InformOfRequest(false);
    }

    event_tracker_.ConnectionAttemptFailed(result);
    event_tracker_.EndConnectionAttempt();

    // If there are other endpoints available, use the next endpoint on the
    // subsequent retry.
    next_endpoint_++;
    if (next_endpoint_ >= mcs_endpoints_.size()) {
      next_endpoint_ = 0;
    }
    connecting_ = false;
    Connect();
    return;
  }

  recorder_->RecordConnectionSuccess();

  // Reset the endpoint back to the default.
  // TODO(zea): consider prioritizing endpoints more intelligently based on
  // which ones succeed most for this client? Although that will affect
  // measuring the success rate of the default endpoint vs fallback.
  last_successful_endpoint_ = next_endpoint_;
  next_endpoint_ = 0;
  connecting_ = false;
  handshake_in_progress_ = true;
  DVLOG(1) << "MCS endpoint socket connection success, starting login.";
  // |peer_addr| is only non-null if result == net::OK and the connection is not
  // through a proxy.
  if (peer_addr) {
    peer_addr_ = peer_addr.value();
  }
  InitHandler(std::move(receive_stream), std::move(send_stream));
}

void ConnectionFactoryImpl::ConnectionHandlerCallback(int result) {
  base::UmaHistogramSparse("GCM.ConnectionHandlerNetCode", -result);
  DCHECK(!connecting_);
  if (result != net::OK) {
    // TODO(zea): Consider how to handle errors that may require some sort of
    // user intervention (login page, etc.).
    LOG(ERROR) << "ConnectionHandler failed with net error: " << result;

    // Failures prior to handshake completion reuse the existing backoff entry.
    if (handshake_in_progress_) {
      backoff_entry_->InformOfRequest(false);
      handshake_in_progress_ = false;
    }
    SignalConnectionReset(SOCKET_FAILURE);
    return;
  }

  // Handshake complete, reset backoff. If the login failed with an error,
  // the client should invoke SignalConnectionReset(LOGIN_FAILURE), which will
  // restore the previous backoff.
  DVLOG(1) << "Handshake complete.";
  handshake_in_progress_ = false;
  last_login_time_ = NowTicks();
  previous_backoff_.swap(backoff_entry_);
  backoff_entry_->Reset();

  event_tracker_.ConnectionAttemptSucceeded();

  if (listener_) {
    listener_->OnConnected(GetCurrentEndpoint(), peer_addr_);
  }
}

void ConnectionFactoryImpl::CloseSocket() {
  // The connection handler needs to be reset, else it'll attempt to keep using
  // the destroyed socket.
  if (connection_handler_) {
    connection_handler_->Reset();
  }

  socket_.reset();
  peer_addr_ = net::IPEndPoint();
  handshake_in_progress_ = false;
}

}  // namespace gcm
