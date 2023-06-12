// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_DISPATCHER_H_
#define PPAPI_PROXY_DISPATCHER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "ipc/message_filter.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/interface_list.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/proxy_channel.h"
#include "ppapi/shared_impl/api_id.h"

namespace IPC {
class MessageFilter;
}

namespace ppapi {
namespace proxy {

class VarSerializationRules;

// An interface proxy can represent either end of a cross-process interface
// call. The "source" side is where the call is invoked, and the "target" side
// is where the call ends up being executed.
//
// Plugin side                          | Browser side
// -------------------------------------|--------------------------------------
//                                      |
//    "Source"                          |    "Target"
//    InterfaceProxy ----------------------> InterfaceProxy
//                                      |
//                                      |
//    "Target"                          |    "Source"
//    InterfaceProxy <---------------------- InterfaceProxy
//                                      |
class PPAPI_PROXY_EXPORT Dispatcher : public ProxyChannel {
 public:
  Dispatcher(const Dispatcher&) = delete;
  Dispatcher& operator=(const Dispatcher&) = delete;

  ~Dispatcher() override;

  // Returns true if the dispatcher is on the plugin side, or false if it's the
  // browser side.
  virtual bool IsPlugin() const = 0;

  VarSerializationRules* serialization_rules() const {
    return serialization_rules_.get();
  }

  // Returns a non-owning pointer to the interface proxy for the given ID, or
  // NULL if the ID isn't found. This will create the proxy if it hasn't been
  // created so far.
  InterfaceProxy* GetInterfaceProxy(ApiID id);

  // Adds the given filter to the IO thread.
  void AddIOThreadMessageFilter(scoped_refptr<IPC::MessageFilter> filter);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  PP_GetInterface_Func local_get_interface() const {
    return local_get_interface_;
  }

  const PpapiPermissions& permissions() const { return permissions_; }

 protected:
  explicit Dispatcher(PP_GetInterface_Func local_get_interface,
                      const PpapiPermissions& permissions);

  // Setter for the derived classes to set the appropriate var serialization.
  // Takes one reference of the given pointer, which must be on the heap.
  void SetSerializationRules(VarSerializationRules* var_serialization_rules);

  // Called when an invalid message is received from the remote site. The
  // default implementation does nothing, derived classes can override.
  virtual void OnInvalidMessageReceived();

 private:
  friend class PluginDispatcherTest;

  // Lists all lazily-created interface proxies.
  std::unique_ptr<InterfaceProxy> proxies_[API_ID_COUNT];

  PP_GetInterface_Func local_get_interface_;

  scoped_refptr<VarSerializationRules> serialization_rules_;

  PpapiPermissions permissions_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_DISPATCHER_H_
