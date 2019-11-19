// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "net/base/features.h"
#include "net/http/http_proxy_connect_job.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/connect_job.h"
#include "net/socket/socks_connect_job.h"
#include "net/socket/ssl_connect_job.h"
#include "net/socket/stream_socket.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"

namespace net {

namespace {

// The maximum duration, in seconds, to keep used idle persistent sockets alive.
int64_t g_used_idle_socket_timeout_s = 300;  // 5 minutes

// Invoked by the transport socket pool after host resolution is complete
// to allow the connection to be aborted, if a matching SPDY session can
// be found. Returns OnHostResolutionCallbackResult::kMayBeDeletedAsync if such
// a session is found, as it will post a task that may delete the calling
// ConnectJob. Also returns kMayBeDeletedAsync if there may already be such
// a task posted.
OnHostResolutionCallbackResult OnHostResolution(
    SpdySessionPool* spdy_session_pool,
    const SpdySessionKey& spdy_session_key,
    bool is_for_websockets,
    const HostPortPair& host_port_pair,
    const AddressList& addresses) {
  DCHECK(host_port_pair == spdy_session_key.host_port_pair());

  // It is OK to dereference spdy_session_pool, because the
  // ClientSocketPoolManager will be destroyed in the same callback that
  // destroys the SpdySessionPool.
  return spdy_session_pool->OnHostResolutionComplete(
      spdy_session_key, is_for_websockets, addresses);
}

}  // namespace

ClientSocketPool::SocketParams::SocketParams(
    std::unique_ptr<SSLConfig> ssl_config_for_origin,
    std::unique_ptr<SSLConfig> ssl_config_for_proxy)
    : ssl_config_for_origin_(std::move(ssl_config_for_origin)),
      ssl_config_for_proxy_(std::move(ssl_config_for_proxy)) {}

ClientSocketPool::SocketParams::~SocketParams() = default;

scoped_refptr<ClientSocketPool::SocketParams>
ClientSocketPool::SocketParams::CreateForHttpForTesting() {
  return base::MakeRefCounted<SocketParams>(nullptr /* ssl_config_for_origin */,
                                            nullptr /* ssl_config_for_proxy */);
}

ClientSocketPool::GroupId::GroupId()
    : socket_type_(SocketType::kHttp),
      privacy_mode_(PrivacyMode::PRIVACY_MODE_DISABLED) {}

ClientSocketPool::GroupId::GroupId(const HostPortPair& destination,
                                   SocketType socket_type,
                                   PrivacyMode privacy_mode,
                                   NetworkIsolationKey network_isolation_key,
                                   bool disable_secure_dns)
    : destination_(destination),
      socket_type_(socket_type),
      privacy_mode_(privacy_mode),
      network_isolation_key_(
          base::FeatureList::IsEnabled(
              features::kPartitionConnectionsByNetworkIsolationKey)
              ? network_isolation_key
              : NetworkIsolationKey()),
      disable_secure_dns_(disable_secure_dns) {}

ClientSocketPool::GroupId::GroupId(const GroupId& group_id) = default;

ClientSocketPool::GroupId::~GroupId() = default;

ClientSocketPool::GroupId& ClientSocketPool::GroupId::operator=(
    const GroupId& group_id) = default;

ClientSocketPool::GroupId& ClientSocketPool::GroupId::operator=(
    GroupId&& group_id) = default;

std::string ClientSocketPool::GroupId::ToString() const {
  std::string result = destination_.ToString();
  switch (socket_type_) {
    case ClientSocketPool::SocketType::kHttp:
      break;

    case ClientSocketPool::SocketType::kSsl:
      result = "ssl/" + result;
      break;
  }
  if (privacy_mode_)
    result = "pm/" + result;

  if (base::FeatureList::IsEnabled(
          features::kPartitionConnectionsByNetworkIsolationKey)) {
    result += " <";
    result += network_isolation_key_.ToDebugString();
    result += ">";
  }

  if (disable_secure_dns_)
    result = "dsd/" + result;

  return result;
}

ClientSocketPool::~ClientSocketPool() = default;

// static
base::TimeDelta ClientSocketPool::used_idle_socket_timeout() {
  return base::TimeDelta::FromSeconds(g_used_idle_socket_timeout_s);
}

// static
void ClientSocketPool::set_used_idle_socket_timeout(base::TimeDelta timeout) {
  DCHECK_GT(timeout.InSeconds(), 0);
  g_used_idle_socket_timeout_s = timeout.InSeconds();
}

ClientSocketPool::ClientSocketPool() = default;

void ClientSocketPool::NetLogTcpClientSocketPoolRequestedSocket(
    const NetLogWithSource& net_log,
    const GroupId& group_id) {
  if (net_log.IsCapturing()) {
    // TODO(eroman): Split out the host and port parameters.
    net_log.AddEvent(NetLogEventType::TCP_CLIENT_SOCKET_POOL_REQUESTED_SOCKET,
                     [&] { return NetLogGroupIdParams(group_id); });
  }
}

base::Value ClientSocketPool::NetLogGroupIdParams(const GroupId& group_id) {
  base::Value event_params(base::Value::Type::DICTIONARY);
  event_params.SetStringKey("group_id", group_id.ToString());
  return event_params;
}

std::unique_ptr<ConnectJob> ClientSocketPool::CreateConnectJob(
    GroupId group_id,
    scoped_refptr<SocketParams> socket_params,
    const ProxyServer& proxy_server,
    const base::Optional<NetworkTrafficAnnotationTag>& proxy_annotation_tag,
    bool is_for_websockets,
    const CommonConnectJobParams* common_connect_job_params,
    RequestPriority request_priority,
    SocketTag socket_tag,
    ConnectJob::Delegate* delegate) {
  bool using_ssl = group_id.socket_type() == ClientSocketPool::SocketType::kSsl;

  // If applicable, set up a callback to handle checking for H2 IP pooling
  // opportunities.
  OnHostResolutionCallback resolution_callback;
  if (using_ssl && proxy_server.is_direct()) {
    resolution_callback = base::BindRepeating(
        &OnHostResolution, common_connect_job_params->spdy_session_pool,
        SpdySessionKey(
            group_id.destination(), proxy_server, group_id.privacy_mode(),
            SpdySessionKey::IsProxySession::kFalse, socket_tag,
            group_id.network_isolation_key(), group_id.disable_secure_dns()),
        is_for_websockets);
  } else if (proxy_server.is_https()) {
    resolution_callback = base::BindRepeating(
        &OnHostResolution, common_connect_job_params->spdy_session_pool,
        SpdySessionKey(proxy_server.host_port_pair(), ProxyServer::Direct(),
                       group_id.privacy_mode(),
                       SpdySessionKey::IsProxySession::kTrue, socket_tag,
                       group_id.network_isolation_key(),
                       group_id.disable_secure_dns()),
        is_for_websockets);
  }

  return ConnectJob::CreateConnectJob(
      using_ssl, group_id.destination(), proxy_server, proxy_annotation_tag,
      socket_params->ssl_config_for_origin(),
      socket_params->ssl_config_for_proxy(), is_for_websockets,
      group_id.privacy_mode(), resolution_callback, request_priority,
      socket_tag, group_id.network_isolation_key(),
      group_id.disable_secure_dns(), common_connect_job_params, delegate);
}

}  // namespace net
