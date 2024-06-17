// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_handle.h"

#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "net/base/net_errors.h"
#include "net/base/trace_constants.h"
#include "net/base/tracing.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/connect_job.h"

namespace net {

ClientSocketHandle::ClientSocketHandle()
    : resolve_error_info_(ResolveErrorInfo(OK)) {}

ClientSocketHandle::~ClientSocketHandle() {
  weak_factory_.InvalidateWeakPtrs();
  Reset();
}

int ClientSocketHandle::Init(
    const ClientSocketPool::GroupId& group_id,
    scoped_refptr<ClientSocketPool::SocketParams> socket_params,
    const std::optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    RequestPriority priority,
    const SocketTag& socket_tag,
    ClientSocketPool::RespectLimits respect_limits,
    CompletionOnceCallback callback,
    const ClientSocketPool::ProxyAuthCallback& proxy_auth_callback,
    ClientSocketPool* pool,
    const NetLogWithSource& net_log) {
  requesting_source_ = net_log.source();

  CHECK(group_id.destination().IsValid());
  ResetInternal(true /* cancel */, false /* cancel_connect_job */);
  ResetErrorState();
  pool_ = pool;
  group_id_ = group_id;
  CompletionOnceCallback io_complete_callback =
      base::BindOnce(&ClientSocketHandle::OnIOComplete, base::Unretained(this));
  int rv = pool_->RequestSocket(
      group_id, std::move(socket_params), proxy_annotation_tag, priority,
      socket_tag, respect_limits, this, std::move(io_complete_callback),
      proxy_auth_callback, net_log);
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
  } else {
    HandleInitCompletion(rv);
  }
  return rv;
}

void ClientSocketHandle::SetPriority(RequestPriority priority) {
  if (socket()) {
    // The priority of the handle is no longer relevant to the socket pool;
    // just return.
    return;
  }

  if (pool_)
    pool_->SetPriority(group_id_, this, priority);
}

void ClientSocketHandle::Reset() {
  ResetInternal(true /* cancel */, false /* cancel_connect_job */);
  ResetErrorState();
}

void ClientSocketHandle::ResetAndCloseSocket() {
  if (is_initialized() && socket()) {
    socket()->Disconnect();
  }
  ResetInternal(true /* cancel */, true /* cancel_connect_job */);
  ResetErrorState();
}

LoadState ClientSocketHandle::GetLoadState() const {
  CHECK(!is_initialized());
  CHECK(group_id_.destination().IsValid());
  // Because of http://crbug.com/37810  we may not have a pool, but have
  // just a raw socket.
  if (!pool_)
    return LOAD_STATE_IDLE;
  return pool_->GetLoadState(group_id_, this);
}

bool ClientSocketHandle::IsPoolStalled() const {
  if (!pool_)
    return false;
  return pool_->IsStalled();
}

void ClientSocketHandle::AddHigherLayeredPool(HigherLayeredPool* higher_pool) {
  CHECK(higher_pool);
  CHECK(!higher_pool_);
  // TODO(mmenke):  |pool_| should only be NULL in tests.  Maybe stop doing that
  // so this be be made into a DCHECK, and the same can be done in
  // RemoveHigherLayeredPool?
  if (pool_) {
    pool_->AddHigherLayeredPool(higher_pool);
    higher_pool_ = higher_pool;
  }
}

void ClientSocketHandle::RemoveHigherLayeredPool(
    HigherLayeredPool* higher_pool) {
  CHECK(higher_pool_);
  CHECK_EQ(higher_pool_, higher_pool);
  if (pool_) {
    pool_->RemoveHigherLayeredPool(higher_pool);
    higher_pool_ = nullptr;
  }
}

void ClientSocketHandle::CloseIdleSocketsInGroup(
    const char* net_log_reason_utf8) {
  if (pool_)
    pool_->CloseIdleSocketsInGroup(group_id_, net_log_reason_utf8);
}

void ClientSocketHandle::SetAdditionalErrorState(ConnectJob* connect_job) {
  connection_attempts_ = connect_job->GetConnectionAttempts();

  resolve_error_info_ = connect_job->GetResolveErrorInfo();
  is_ssl_error_ = connect_job->IsSSLError();
  ssl_cert_request_info_ = connect_job->GetCertRequestInfo();
}

void ClientSocketHandle::OnIOComplete(int result) {
  TRACE_EVENT0(NetTracingCategory(), "ClientSocketHandle::OnIOComplete");
  CompletionOnceCallback callback = std::move(callback_);
  callback_.Reset();
  HandleInitCompletion(result);
  std::move(callback).Run(result);
}

void ClientSocketHandle::HandleInitCompletion(int result) {
  CHECK_NE(ERR_IO_PENDING, result);
  if (result != OK) {
    if (!socket()) {
      ResetInternal(false /* cancel */,
                    false /* cancel_connect_job */);  // Nothing to cancel since
                                                      // the request failed.
    } else {
      set_is_initialized(true);
    }
    return;
  }
  set_is_initialized(true);
  CHECK_NE(-1, group_generation_)
      << "Pool should have set |group_generation_| to a valid value.";

  // Broadcast that the socket has been acquired.
  // TODO(eroman): This logging is not complete, in particular set_socket() and
  // release() socket. It ends up working though, since those methods are being
  // used to layer sockets (and the destination sources are the same).
  DCHECK(socket());
  socket()->NetLog().BeginEventReferencingSource(NetLogEventType::SOCKET_IN_USE,
                                                 requesting_source_);
}

void ClientSocketHandle::ResetInternal(bool cancel, bool cancel_connect_job) {
  DCHECK(cancel || !cancel_connect_job);

  // Was Init called?
  if (group_id_.destination().IsValid()) {
    // If so, we must have a pool.
    CHECK(pool_);
    if (is_initialized()) {
      if (socket()) {
        socket()->NetLog().EndEvent(NetLogEventType::SOCKET_IN_USE);
        // Release the socket back to the ClientSocketPool so it can be
        // deleted or reused.
        pool_->ReleaseSocket(group_id_, PassSocket(), group_generation_);
      } else {
        // If the handle has been initialized, we should still have a
        // socket.
        NOTREACHED_IN_MIGRATION();
      }
    } else if (cancel) {
      // If we did not get initialized yet and we have a socket
      // request pending, cancel it.
      pool_->CancelRequest(group_id_, this, cancel_connect_job);
    }
  }
  set_is_initialized(false);
  PassSocket();
  group_id_ = ClientSocketPool::GroupId();
  set_reuse_type(SocketReuseType::kUnused);
  callback_.Reset();
  if (higher_pool_)
    RemoveHigherLayeredPool(higher_pool_);
  pool_ = nullptr;
  idle_time_ = base::TimeDelta();
  set_connect_timing(LoadTimingInfo::ConnectTiming());
  group_generation_ = -1;
}

void ClientSocketHandle::ResetErrorState() {
  resolve_error_info_ = ResolveErrorInfo(OK);
  is_ssl_error_ = false;
  ssl_cert_request_info_ = nullptr;
}

}  // namespace net
