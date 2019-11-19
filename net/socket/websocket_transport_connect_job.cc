// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_transport_connect_job.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/socket/transport_connect_job.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/socket/websocket_transport_connect_sub_job.h"

namespace net {

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
      next_state_(STATE_NONE),
      race_result_(TransportConnectJob::RACE_UNKNOWN),
      had_ipv4_(false),
      had_ipv6_(false) {
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
  DCHECK(!params_->disable_secure_dns());
  request_ = host_resolver()->CreateRequest(params_->destination(),
                                            params_->network_isolation_key(),
                                            net_log(), parameters);

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

  if (result != OK)
    return result;
  DCHECK(request_->GetAddressResults());

  next_state_ = STATE_TRANSPORT_CONNECT;

  // Invoke callback.  If it indicates |this| may be slated for deletion, then
  // only continue after a PostTask.
  if (!params_->host_resolution_callback().is_null()) {
    OnHostResolutionCallbackResult callback_result =
        params_->host_resolution_callback().Run(
            params_->destination(), request_->GetAddressResults().value());
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
  int result = ERR_UNEXPECTED;
  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;

  for (AddressList::const_iterator it =
           request_->GetAddressResults().value().begin();
       it != request_->GetAddressResults().value().end(); ++it) {
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
    had_ipv4_ = true;
    ipv4_job_.reset(new WebSocketTransportConnectSubJob(
        ipv4_addresses, this, SUB_JOB_IPV4, websocket_endpoint_lock_manager()));
  }

  if (!ipv6_addresses.empty()) {
    had_ipv6_ = true;
    ipv6_job_.reset(new WebSocketTransportConnectSubJob(
        ipv6_addresses, this, SUB_JOB_IPV6, websocket_endpoint_lock_manager()));
    result = ipv6_job_->Start();
    switch (result) {
      case OK:
        SetSocket(ipv6_job_->PassSocket());
        race_result_ = had_ipv4_ ? TransportConnectJob::RACE_IPV6_WINS
                                 : TransportConnectJob::RACE_IPV6_SOLO;
        return result;

      case ERR_IO_PENDING:
        if (ipv4_job_) {
          // This use of base::Unretained is safe because |fallback_timer_| is
          // owned by this object.
          fallback_timer_.Start(
              FROM_HERE,
              base::TimeDelta::FromMilliseconds(
                  TransportConnectJob::kIPv6FallbackTimerInMs),
              base::BindOnce(&WebSocketTransportConnectJob::StartIPv4JobAsync,
                             base::Unretained(this)));
        }
        return result;

      default:
        ipv6_job_.reset();
    }
  }

  DCHECK(!ipv6_job_);
  if (ipv4_job_) {
    result = ipv4_job_->Start();
    if (result == OK) {
      SetSocket(ipv4_job_->PassSocket());
      race_result_ = had_ipv6_ ? TransportConnectJob::RACE_IPV4_WINS
                               : TransportConnectJob::RACE_IPV4_SOLO;
    }
  }

  return result;
}

int WebSocketTransportConnectJob::DoTransportConnectComplete(int result) {
  if (result == OK)
    TransportConnectJob::HistogramDuration(connect_timing_, race_result_);
  return result;
}

void WebSocketTransportConnectJob::OnSubJobComplete(
    int result,
    WebSocketTransportConnectSubJob* job) {
  if (result == OK) {
    switch (job->type()) {
      case SUB_JOB_IPV4:
        race_result_ = had_ipv6_ ? TransportConnectJob::RACE_IPV4_WINS
                                 : TransportConnectJob::RACE_IPV4_SOLO;
        break;

      case SUB_JOB_IPV6:
        race_result_ = had_ipv4_ ? TransportConnectJob::RACE_IPV6_WINS
                                 : TransportConnectJob::RACE_IPV6_SOLO;
        break;
    }
    SetSocket(job->PassSocket());

    // Make sure all connections are cancelled even if this object fails to be
    // deleted.
    ipv4_job_.reset();
    ipv6_job_.reset();
  } else {
    switch (job->type()) {
      case SUB_JOB_IPV4:
        ipv4_job_.reset();
        break;

      case SUB_JOB_IPV6:
        ipv6_job_.reset();
        if (ipv4_job_ && !ipv4_job_->started()) {
          fallback_timer_.Stop();
          result = ipv4_job_->Start();
          if (result != ERR_IO_PENDING) {
            OnSubJobComplete(result, ipv4_job_.get());
            return;
          }
        }
        break;
    }
    if (ipv4_job_ || ipv6_job_)
      return;
  }
  OnIOComplete(result);
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
