// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MESSAGE_ROUTER_H_
#define IPC_MESSAGE_ROUTER_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/id_map.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

// The MessageRouter handles all incoming messages sent to it by routing them
// to the correct listener.  Routing is based on the Message's routing ID.
// Since routing IDs are typically assigned asynchronously by the browser
// process, the MessageRouter has the notion of pending IDs for listeners that
// have not yet been assigned a routing ID.
//
// When a message arrives, the routing ID is used to index the set of routes to
// find a listener.  If a listener is found, then the message is passed to it.
// Otherwise, the message is ignored if its routing ID is not equal to
// MSG_ROUTING_CONTROL.
//
// The MessageRouter supports the IPC::Sender interface for outgoing messages,
// but does not define a meaningful implementation of it.  The subclass of
// MessageRouter is intended to provide that if appropriate.
//
// The MessageRouter can be used as a concrete class provided its Send method
// is not called and it does not receive any control messages.

namespace IPC {

class COMPONENT_EXPORT(IPC) MessageRouter : public Listener, public Sender {
 public:
  MessageRouter();

  MessageRouter(const MessageRouter&) = delete;
  MessageRouter& operator=(const MessageRouter&) = delete;

  ~MessageRouter() override;

  // Implemented by subclasses to handle control messages
  virtual bool OnControlMessageReceived(const Message& msg);

  // Listener implementation:
  bool OnMessageReceived(const Message& msg) override;

  // Like OnMessageReceived, except it only handles routed messages.  Returns
  // true if the message was dispatched, or false if there was no listener for
  // that route id.
  virtual bool RouteMessage(const Message& msg);

  // Sender implementation:
  bool Send(Message* msg) override;

  // Called to add a listener for a particular message routing ID.
  // Returns true if succeeded.
  bool AddRoute(int32_t routing_id, Listener* listener);

  // Called to remove a listener for a particular message routing ID.
  void RemoveRoute(int32_t routing_id);

  // Returns the Listener associated with |routing_id|.
  Listener* GetRoute(int32_t routing_id);

 private:
  // A list of all listeners with assigned routing IDs.
  base::IDMap<Listener*> routes_;
};

}  // namespace IPC

#endif  // IPC_MESSAGE_ROUTER_H_
