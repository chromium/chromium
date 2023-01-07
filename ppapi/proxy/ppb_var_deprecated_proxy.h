// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_VAR_DEPRECATED_PROXY_H_
#define PPAPI_PROXY_PPB_VAR_DEPRECATED_PROXY_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Var_Deprecated;

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;
class SerializedVarVectorOutParam;
class SerializedVarVectorReceiveInput;
class SerializedVarOutParam;
class SerializedVarReturnValue;

class PPB_Var_Deprecated_Proxy : public InterfaceProxy {
 public:
  explicit PPB_Var_Deprecated_Proxy(Dispatcher* dispatcher);

  PPB_Var_Deprecated_Proxy(const PPB_Var_Deprecated_Proxy&) = delete;
  PPB_Var_Deprecated_Proxy& operator=(const PPB_Var_Deprecated_Proxy&) = delete;

  ~PPB_Var_Deprecated_Proxy() override;

  static const PPB_Var_Deprecated* GetProxyInterface();

  // InterfaceProxy implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  // Message handlers.
  void OnMsgAddRefObject(int64_t object_id);
  void OnMsgReleaseObject(int64_t object_id);
  void OnMsgHasProperty(SerializedVarReceiveInput var,
                        SerializedVarReceiveInput name,
                        SerializedVarOutParam exception,
                        PP_Bool* result);
  void OnMsgHasMethodDeprecated(SerializedVarReceiveInput var,
                                SerializedVarReceiveInput name,
                                SerializedVarOutParam exception,
                                PP_Bool* result);
  void OnMsgGetProperty(SerializedVarReceiveInput var,
                        SerializedVarReceiveInput name,
                        SerializedVarOutParam exception,
                        SerializedVarReturnValue result);
  void OnMsgEnumerateProperties(
      SerializedVarReceiveInput var,
      SerializedVarVectorOutParam props,
      SerializedVarOutParam exception);
  void OnMsgSetPropertyDeprecated(SerializedVarReceiveInput var,
                                  SerializedVarReceiveInput name,
                                  SerializedVarReceiveInput value,
                                  SerializedVarOutParam exception);
  void OnMsgDeleteProperty(SerializedVarReceiveInput var,
                           SerializedVarReceiveInput name,
                           SerializedVarOutParam exception,
                           PP_Bool* result);
  void OnMsgCall(SerializedVarReceiveInput object,
                 SerializedVarReceiveInput this_object,
                 SerializedVarReceiveInput method_name,
                 SerializedVarVectorReceiveInput arg_vector,
                 SerializedVarOutParam exception,
                 SerializedVarReturnValue result);
  void OnMsgCallDeprecated(SerializedVarReceiveInput object,
                           SerializedVarReceiveInput method_name,
                           SerializedVarVectorReceiveInput arg_vector,
                           SerializedVarOutParam exception,
                           SerializedVarReturnValue result);
  void OnMsgConstruct(SerializedVarReceiveInput var,
                      SerializedVarVectorReceiveInput arg_vector,
                      SerializedVarOutParam exception,
                      SerializedVarReturnValue result);
  void OnMsgIsInstanceOfDeprecated(SerializedVarReceiveInput var,
                                   int64_t ppp_class,
                                   int64_t* ppp_class_data,
                                   PP_Bool* result);
  void OnMsgCreateObjectDeprecated(PP_Instance instance,
                                   int64_t ppp_class,
                                   int64_t ppp_class_data,
                                   SerializedVarReturnValue result);

  // Call in the host for messages that can be reentered.
  void SetAllowPluginReentrancy();

  void DoReleaseObject(int64_t object_id);

  const PPB_Var_Deprecated* ppb_var_impl_;

  base::WeakPtrFactory<PPB_Var_Deprecated_Proxy> task_factory_{this};
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_VAR_DEPRECATED_PROXY_H_
