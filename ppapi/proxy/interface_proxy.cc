// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/interface_proxy.h"

#include "ppapi/proxy/dispatcher.h"

namespace ppapi {
namespace proxy {

InterfaceProxy::InterfaceProxy(Dispatcher* dispatcher)
    : dispatcher_(dispatcher) {
}

InterfaceProxy::~InterfaceProxy() {
}

bool InterfaceProxy::Send(IPC::Message* msg) {
  return dispatcher_->Send(msg);
}

}  // namespace proxy
}  // namespace ppapi
