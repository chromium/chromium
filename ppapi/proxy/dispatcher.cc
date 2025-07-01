// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/dispatcher.h"

#include <string.h>  // For memset.

#include <map>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/var_serialization_rules.h"

namespace IPC {
class MessageFilter;
}

namespace ppapi {
namespace proxy {

Dispatcher::Dispatcher(PP_GetInterface_Func local_get_interface,
                       const PpapiPermissions& permissions)
    : local_get_interface_(local_get_interface),
      permissions_(permissions) {
}

Dispatcher::~Dispatcher() {
}

InterfaceProxy* Dispatcher::GetInterfaceProxy(ApiID id) {
  InterfaceProxy* proxy = proxies_[id].get();
  if (!proxy) {
    // Handle the first time for a given API by creating the proxy for it.
    InterfaceProxy::Factory factory =
        InterfaceList::GetInstance()->GetFactoryForID(id);
    CHECK(factory);
    proxy = factory(this);
    DCHECK(proxy);
    proxies_[id].reset(proxy);
  }
  return proxy;
}

void Dispatcher::AddIOThreadMessageFilter(
    scoped_refptr<IPC::MessageFilter> filter) {
  // Our filter is refcounted. The channel will call the destruct method on the
  // filter when the channel is done with it, so the corresponding Release()
  // happens there.
  channel()->AddFilter(filter.get());
}

bool Dispatcher::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() <= 0 || msg.routing_id() >= API_ID_COUNT) {
    OnInvalidMessageReceived();
    return true;
  }

  InterfaceProxy* proxy = GetInterfaceProxy(
      static_cast<ApiID>(msg.routing_id()));
  CHECK(proxy);
  return proxy->OnMessageReceived(msg);
}

void Dispatcher::SetSerializationRules(
    VarSerializationRules* var_serialization_rules) {
  serialization_rules_ = var_serialization_rules;
}

void Dispatcher::OnInvalidMessageReceived() {
}

}  // namespace proxy
}  // namespace ppapi
