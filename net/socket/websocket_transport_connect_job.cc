// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_transport_connect_job.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/socket/socket_tag.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/socket/websocket_transport_connect_sub_job.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

// TODO(crbug.com/1206799): Delete once endpoint usage is converted to using
// url::SchemeHostPort when available.
HostPortPair ToLegacyDestinationEndpoint(
    const TransportSocketParams::Endpoint& endpoint) {
  if (absl::holds_alternative<url::SchemeHostPort>(endpoint)) {
    return HostPortPair::FromSchemeHostPort(
        absl::get<url::SchemeHostPort>(endpoint));
  }

  DCHECK(absl::holds_alternative<HostPortPair>(endpoint));
  return absl::get<HostPortPair>(endpoint);
}

}  // namespace

std::unique_ptr<WebSocketTransportConnectJob>
WebSocketTransportConnectJob::Factory::Create(
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    const scoped_refptr<TransportSocketParams>& params,
    Delegate* delegate,
    const NetLogWithSource* net_log) {
  return std::make_unique<WebSocketTransportConnectJob>(
      priority, socket_tag, common_connect_job_params, params, delegate,
      net_log);
}

WebSocketTransportConnectJob::WebSocketTransportConnectJob(
    RequestPriority priority,
    const SocketTag& socket_tag,
    const CommonConnectJobParams* common_connect_job_params,
    const scoped_refptr<TransportSocketParams>& params,
    Delegate* delegate,
    const NetLogWithSource* net_log)
    : ConnectJob(priority,
                 socket_tag,
                 TransportConnectJob::ConnectionTimeout(),
                 common_connect_job_params,
                 delegate,
                 net_log,
                 NetLogSourceType::WEB_SOCKET_TRANSPORT_CONNECT_JOB,
                 NetLogEventType::WEB_SOCKET_TRANSPORT_CONNECT_JOB_CONNECT),
      params_(params),
      next_state_(STATE_NONE) {
  DCHECK(common_connect_job_params->websocket_endpoint_lock_manager);
}

WebSocketTransportConnectJob::~WebSocketTransportConnectJob() = default;

LoadState WebSocketTransportConnectJob::GetLoadState() const {
  LoadState load_state = LOAD_STATE_RESOLVING_HOST;
  if (ipv6_job_)
    load_state = ipv6_job_->GetLoadState();
  // This method should return LOAD_STATE_CONNECTING in preference to
  // LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET when possible because "waiting for
  // available socket" implies that nothing is happening.
  if (ipv4_job_ && load_state != LOAD_STATE_CONNECTING)
    load_state = ipv4_job_->GetLoadState();
  return load_state;
}

bool WebSocketTransportConnectJob::HasEstablishedConnection() const {
  // No need to ever return true, since NotifyComplete() is called as soon as a
  // connection is established.
  return false;
}

ConnectionAttempts WebSocketTransportConnectJob::GetConnectionAttempts() const {
  return connection_attempts_;
}

ResolveErrorInfo WebSocketTransportConnectJob::GetResolveErrorInfo() const {
  return resolve_error_info_;
}

void WebSocketTransportConnectJob::OnIOComplete(int result) {
  result = DoLoop(result);
  if (result != ERR_IO_PENDING)
    NotifyDelegateOfCompletion(result);  // Deletes |this|
}

int WebSocketTransportConnectJob::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_RESOLVE_HOST:
        DCHECK_EQ(OK, rv);
        rv = DoResolveHost();
        break;
      case STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case STATE_TRANSPORT_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTransportConnect();
        break;
      case STATE_TRANSPORT_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(rv);
        break;
      default:
        NOTREACHED();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);

  return rv;
}

int WebSocketTransportConnectJob::DoResolveHost() {
  next_state_ = STATE_RESOLVE_HOST_COMPLETE;
  connect_timing_.dns_start = base::TimeTicks::Now();

  HostResolver::ResolveHostParameters parameters;
  parameters.initial_priority = priority();
  DCHECK_EQ(SecureDnsPolicy::kAllow, params_->secure_dns_policy());
  if (absl::holds_alternative<url::SchemeHostPort>(params_->destination())) {
    request_ = host_resolver()->CreateRequest(
        absl::get<url::SchemeHostPort>(params_->destination()),
        params_->network_isolation_key(), net_log(), parameters);
  } else {
    request_ = host_resolver()->CreateRequest(
        absl::get<HostPortPair>(params_->destination()),
        params_->network_isolation_key(), net_log(), parameters);
  }

  return request_->Start(base::BindOnce(
      &WebSocketTransportConnectJob::OnIOComplete, base::Unretained(this)));
}

int WebSocketTransportConnectJob::DoResolveHostComplete(int result) {
  TRACE_EVENT0(NetTracingCategory(),
               "WebSocketTransportConnectJob::DoResolveHostComplete");
  connect_timing_.dns_end = base::TimeTicks::Now();
  // Overwrite connection start time, since for connections that do not go
  // through proxies, |connect_start| should not include dns lookup time.
  connect_timing_.connect_start = connect_timing_.dns_end;
  resolve_error_info_ = request_->GetResolveErrorInfo();

  if (result != OK) {
    // If hostname resolution failed, record an empty endpoint and the result.
    connection_attempts_.push_back(ConnectionAttempt(IPEndPoint(), result));
    return result;
  }
  DCHECK(request_->GetAddressResults());

  next_state_ = STATE_TRANSPORT_CONNECT;

  // Invoke callback.  If it indicates |this| may be slated for deletion, then
  // only continue after a PostTask.
  if (!params_->host_resolution_callback().is_null()) {
    OnHostResolutionCallbackResult callback_result =
        params_->host_resolution_callback().Run(
            ToLegacyDestinationEndpoint(params_->destination()),
            *request_->GetAddressResults());
    if (callback_result == OnHostResolutionCallbackResult::kMayBeDeletedAsync) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&WebSocketTransportConnectJob::OnIOComplete,
                                    weak_ptr_factory_.GetWeakPtr(), OK));
      return ERR_IO_PENDING;
    }
  }

  return result;
}

int WebSocketTransportConnectJob::DoTransportConnect() {
  DCHECK(request_->GetAddressResults());

  AddressList ipv4_addresses;
  AddressList ipv6_addresses;
  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;

  for (AddressList::const_iterator it = request_->GetAddressResults()->begin();
       it != request_->GetAddressResults()->end(); ++it) {
    switch (it->GetFamily()) {
      case ADDRESS_FAMILY_IPV4:
        ipv4_addresses.push_back(*it);
        break;

      case ADDRESS_FAMILY_IPV6:
        ipv6_addresses.push_back(*it);
        break;

      default:
        DVLOG(1) << "Unexpected ADDRESS_FAMILY: " << it->GetFamily();
        break;
    }
  }

  if (!ipv4_addresses.empty()) {
    ipv4_job_ = std::make_unique<WebSocketTransportConnectSubJob>(
        ipv4_addresses, this, SUB_JOB_IPV4);
  }

  if (!ipv6_addresses.empty()) {
    ipv6_job_ = std::make_unique<WebSocketTransportConnectSubJob>(
        ipv6_addresses, this, SUB_JOB_IPV6);
    int result = ipv6_job_->Start();
    if (result != ERR_IO_PENDING)
      return HandleSubJobComplete(result, ipv6_job_.get());
    if (ipv4_job_) {
      // This use of base::Unretained is safe because |fallback_timer_| is
      // owned by this object.
      fallback_timer_.Start(
          FROM_HERE, TransportConnectJob::kIPv6FallbackTime,
          base::BindOnce(&WebSocketTransportConnectJob::StartIPv4JobAsync,
                         base::Unretained(this)));
    }
    return ERR_IO_PENDING;
  }

  DCHECK(!ipv6_job_);
  if (ipv4_job_) {
    int result = ipv4_job_->Start();
    if (result != ERR_IO_PENDING)
      return HandleSubJobComplete(result, ipv4_job_.get());
    return ERR_IO_PENDING;
  }

  return ERR_UNEXPECTED;
}

int WebSocketTransportConnectJob::DoTransportConnectComplete(int result) {
  // Make sure nothing else calls back into this object.
  ipv4_job_.reset();
  ipv6_job_.reset();
  fallback_timer_.Stop();

  if (result == OK)
    TransportConnectJob::HistogramDuration(connect_timing_);
  return result;
}

int WebSocketTransportConnectJob::HandleSubJobComplete(
    int result,
    WebSocketTransportConnectSubJob* job) {
  DCHECK_NE(result, ERR_IO_PENDING);
  if (result == OK) {
    DCHECK(request_);
    SetSocket(job->PassSocket(),
              base::OptionalFromPtr(request_->GetDnsAliasResults()));
    return result;
  }

  if (result == ERR_NETWORK_IO_SUSPENDED) {
    // Don't try other jobs if entering suspend mode.
    return result;
  }

  switch (job->type()) {
    case SUB_JOB_IPV4:
      ipv4_job_.reset();
      break;

    case SUB_JOB_IPV6:
      ipv6_job_.reset();
      // Start the other job, rather than wait for the fallback timer.
      if (ipv4_job_ && !ipv4_job_->started()) {
        fallback_timer_.Stop();
        result = ipv4_job_->Start();
        if (result != ERR_IO_PENDING) {
          return HandleSubJobComplete(result, ipv4_job_.get());
        }
      }
      break;
  }

  if (ipv4_job_ || ipv6_job_) {
    // Wait for the other job to complete, rather than reporting |result|.
    return ERR_IO_PENDING;
  }

  return result;
}

void WebSocketTransportConnectJob::OnSubJobComplete(
    int result,
    WebSocketTransportConnectSubJob* job) {
  result = HandleSubJobComplete(result, job);
  if (result != ERR_IO_PENDING) {
    OnIOComplete(result);
  }
}

void WebSocketTransportConnectJob::StartIPv4JobAsync() {
  DCHECK(ipv4_job_);
  int result = ipv4_job_->Start();
  if (result != ERR_IO_PENDING)
    OnSubJobComplete(result, ipv4_job_.get());
}

int WebSocketTransportConnectJob::ConnectInternal() {
  next_state_ = STATE_RESOLVE_HOST;
  return DoLoop(OK);
}

// Nothing to do here because WebSocket priorities are not changed and
// stalled_request_{queue, map} don't take priority into account anyway.
// TODO(chlily): If that ever changes, make the host resolver request reflect
// the new priority.
void WebSocketTransportConnectJob::ChangePriorityInternal(
    RequestPriority priority) {}

}  // namespace net
