// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_VAR_SERIALIZATION_RULES_H_
#define PPAPI_PROXY_PLUGIN_VAR_SERIALIZATION_RULES_H_

#include "base/memory/weak_ptr.h"
#include "ppapi/proxy/var_serialization_rules.h"

namespace ppapi {
namespace proxy {

class PluginDispatcher;
class PluginVarTracker;

// Implementation of the VarSerializationRules interface for the plugin.
class PluginVarSerializationRules : public VarSerializationRules {
 public:
  // This class will use the given non-owning pointer to the var tracker to
  // handle object refcounting and string conversion.
  explicit PluginVarSerializationRules(
      const base::WeakPtr<PluginDispatcher>& dispatcher);

  PluginVarSerializationRules(const PluginVarSerializationRules&) = delete;
  PluginVarSerializationRules& operator=(const PluginVarSerializationRules&) =
      delete;

  ~PluginVarSerializationRules();

  // VarSerialization implementation.
  virtual PP_Var SendCallerOwned(const PP_Var& var);
  virtual PP_Var BeginReceiveCallerOwned(const PP_Var& var);
  virtual void EndReceiveCallerOwned(const PP_Var& var);
  virtual PP_Var ReceivePassRef(const PP_Var& var);
  virtual PP_Var BeginSendPassRef(const PP_Var& var);
  virtual void EndSendPassRef(const PP_Var& var);
  virtual void ReleaseObjectRef(const PP_Var& var);

 private:
  PluginVarTracker* var_tracker_;

  // In most cases, |dispatcher_| won't be NULL, but you should always check
  // before using it.
  // One scenario that it becomes NULL: A SerializedVar holds a ref to this
  // object, and a sync message is issued. While waiting for the reply to the
  // sync message, some incoming message causes the dispatcher to be destroyed.
  // If that happens, we may leak references to object vars. Considering that
  // scripting has been deprecated, this may not be a big issue.
  base::WeakPtr<PluginDispatcher> dispatcher_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_VAR_SERIALIZATION_RULES_H_
