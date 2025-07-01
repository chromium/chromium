// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_INTERFACE_PROXY_H_
#define PPAPI_PROXY_INTERFACE_PROXY_H_

#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/api_id.h"

namespace ppapi {
namespace proxy {

class Dispatcher;

class InterfaceProxy : public IPC::Listener, public IPC::Sender {
 public:
  // Factory function type for interfaces. Ownership of the returned pointer
  // is transferred to the caller.
  typedef InterfaceProxy* (*Factory)(Dispatcher* dispatcher);

  ~InterfaceProxy() override;

  Dispatcher* dispatcher() const { return dispatcher_; }

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

  // Sub-classes must implement IPC::Listener which contains this:
  // virtual bool OnMessageReceived(const Message& message) = 0;

 protected:
  // Creates the given interface associated with the given dispatcher. The
  // dispatcher manages our lifetime.
  explicit InterfaceProxy(Dispatcher* dispatcher);

 private:
  Dispatcher* dispatcher_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_INTERFACE_PROXY_H_

