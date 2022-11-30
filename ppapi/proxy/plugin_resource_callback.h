// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_CALLBACK_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_CALLBACK_H_

#include "ipc/ipc_message.h"
#include "ppapi/proxy/dispatch_reply_message.h"
#include "ppapi/proxy/resource_message_params.h"

namespace ppapi {
namespace proxy {

// |PluginResourceCallback| wraps a |base::OnceCallback| on the plugin side
// which will be triggered in response to a particular message type being
// received. |MsgClass| is the reply message type that the callback will be
// called with and |CallbackType| is the type of the |base::OnceCallback| that
// will be called.
class PluginResourceCallbackBase {
 public:
  virtual ~PluginResourceCallbackBase() = default;

  virtual void Run(const ResourceMessageReplyParams& params,
                   const IPC::Message& msg) = 0;
};

template<typename MsgClass, typename CallbackType>
class PluginResourceCallback : public PluginResourceCallbackBase {
 public:
  explicit PluginResourceCallback(CallbackType callback)
      : callback_(std::move(callback)) {}

  void Run(
      const ResourceMessageReplyParams& reply_params,
      const IPC::Message& msg) override {
    DispatchResourceReplyOrDefaultParams<MsgClass>(
        std::forward<CallbackType>(callback_), reply_params, msg);
  }

 private:
  ~PluginResourceCallback() override {}

  CallbackType callback_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_CALLBACK_H_
