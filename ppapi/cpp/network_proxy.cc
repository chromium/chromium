// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/network_proxy.h"

#include "ppapi/c/ppb_network_proxy.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_NetworkProxy_1_0>() {
  return PPB_NETWORKPROXY_INTERFACE_1_0;
}

}  // namespace

// static
bool NetworkProxy::IsAvailable() {
  return has_interface<PPB_NetworkProxy_1_0>();
}

// static
int32_t NetworkProxy::GetProxyForURL(
    const InstanceHandle& instance,
    const Var& url,
    const CompletionCallbackWithOutput<Var>& callback) {
  if (!has_interface<PPB_NetworkProxy_1_0>())
    return callback.MayForce(PP_ERROR_NOINTERFACE);

  return get_interface<PPB_NetworkProxy_1_0>()->GetProxyForURL(
      instance.pp_instance(), url.pp_var(),
      callback.output(), callback.pp_completion_callback());
}

}  // namespace pp
