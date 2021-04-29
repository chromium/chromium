// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_WEBSOCKET_TRANSPORT_CONNECT_SUB_JOB_H_
#define NET_SOCKET_WEBSOCKET_TRANSPORT_CONNECT_SUB_JOB_H_

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "net/base/address_list.h"
#include "net/base/load_states.h"
#include "net/socket/websocket_endpoint_lock_manager.h"
#include "net/socket/websocket_transport_connect_job.h"

namespace net {

class ClientSocketFactory;
class IPEndPoint;
class NetLogWithSource;
class StreamSocket;
class WebSocketEndpointLockManager;

// Attempts to connect to a subset of the addresses required by a
// WebSocketTransportConnectJob, specifically either the IPv4 or IPv6
// addresses. Each address is tried in turn, and parent_job->OnSubJobComplete()
// is called when the first address succeeds or the last address fails.
class WebSocketTransportConnectSubJob
    : public WebSocketEndpointLockManager::Waiter {
 public:
  typedef WebSocketTransportConnectJob::SubJobType SubJobType;

  WebSocketTransportConnectSubJob(
      const AddressList& addresses,
      WebSocketTransportConnectJob* parent_job,
      SubJobType type,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager);

  ~WebSocketTransportConnectSubJob() override;

  // Start connecting.
  int Start();

  bool started() { return next_state_ != STATE_NONE; }

  LoadState GetLoadState() const;

  SubJobType type() const { return type_; }

  std::unique_ptr<StreamSocket> PassSocket() {
    return std::move(transport_socket_);
  }

  // Implementation of WebSocketEndpointLockManager::EndpointWaiter.
  void GotEndpointLock() override;

 private:
  enum State {
    STATE_NONE,
    STATE_OBTAIN_LOCK,
    STATE_OBTAIN_LOCK_COMPLETE,
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
    STATE_DONE,
  };

  ClientSocketFactory* client_socket_factory() const;

  const NetLogWithSource& net_log() const;

  const IPEndPoint& CurrentAddress() const;

  void OnIOComplete(int result);
  int DoLoop(int result);
  int DoEndpointLock();
  int DoEndpointLockComplete();
  int DoTransportConnect();
  int DoTransportConnectComplete(int result);

  WebSocketTransportConnectJob* const parent_job_;

  const AddressList addresses_;
  size_t current_address_index_;

  State next_state_;
  const SubJobType type_;
  WebSocketEndpointLockManager* const websocket_endpoint_lock_manager_;

  std::unique_ptr<StreamSocket> transport_socket_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketTransportConnectSubJob);
};

}  // namespace net

#endif  // NET_SOCKET_WEBSOCKET_TRANSPORT_CONNECT_SUB_JOB_H_
