// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/message_router.h"

#include "base/logging.h"
#include "ipc/ipc_message.h"

namespace IPC {

MessageRouter::MessageRouter() = default;

MessageRouter::~MessageRouter() = default;

bool MessageRouter::OnControlMessageReceived(const IPC::Message& msg) {
  NOTREACHED()
      << "should override in subclass if you care about control messages";
}

bool MessageRouter::Send(IPC::Message* msg) {
  NOTREACHED()
      << "should override in subclass if you care about sending messages";
}

bool MessageRouter::AddRoute(int32_t routing_id, IPC::Listener* listener) {
  if (routes_.Lookup(routing_id)) {
    DLOG(ERROR) << "duplicate routing ID";
    return false;
  }
  routes_.AddWithID(listener, routing_id);
  return true;
}

void MessageRouter::RemoveRoute(int32_t routing_id) {
  routes_.Remove(routing_id);
}

Listener* MessageRouter::GetRoute(int32_t routing_id) {
  return routes_.Lookup(routing_id);
}

bool MessageRouter::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL)
    return OnControlMessageReceived(msg);

  return RouteMessage(msg);
}

bool MessageRouter::RouteMessage(const IPC::Message& msg) {
  IPC::Listener* listener = routes_.Lookup(msg.routing_id());
  if (!listener)
    return false;

  return listener->OnMessageReceived(msg);
}

}  // namespace IPC
