// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PROXY_OBJECT_VAR_H_
#define PPAPI_PROXY_PROXY_OBJECT_VAR_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

namespace proxy {
class PluginDispatcher;
}  // namespace proxy

// Tracks a reference to an object var in the plugin side of the proxy. This
// just stores the dispatcher and host var ID, and provides the interface for
// integrating this with PP_Var creation.
class PPAPI_PROXY_EXPORT ProxyObjectVar : public Var {
 public:
  ProxyObjectVar(proxy::PluginDispatcher* dispatcher, int32_t host_var_id);

  ProxyObjectVar(const ProxyObjectVar&) = delete;
  ProxyObjectVar& operator=(const ProxyObjectVar&) = delete;

  ~ProxyObjectVar() override;

  // Var overrides.
  ProxyObjectVar* AsProxyObjectVar() override;
  PP_VarType GetType() const override;

  proxy::PluginDispatcher* dispatcher() const { return dispatcher_; }
  int32_t host_var_id() const { return host_var_id_; }

  void* user_data() const { return user_data_; }
  void set_user_data(void* ud) { user_data_ = ud; }

  // Expose AssignVarID on Var so the PluginResourceTracker can call us when
  // it's creating IDs.
  void AssignVarID(int32_t id);

  void clear_dispatcher() { dispatcher_ = NULL; }

 private:
  proxy::PluginDispatcher* dispatcher_;
  int32_t host_var_id_;

  // When this object is created as representing a var implemented by the
  // plugin, this stores the user data so that we can look it up later. See
  // PluginVarTracker.
  void* user_data_;
};

}  // namespace ppapi

#endif  // PPAPI_PROXY_PROXY_OBJECT_VAR_H_
