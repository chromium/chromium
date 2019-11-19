// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_ROUTER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_ROUTER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/resource_message_params.h"

namespace IPC {
class Message;
class MessageReplyDeserializer;
}  // namespace

namespace content {

class RendererPpapiHostImpl;

// This class fakes an IPC channel so that we can take the new resources with
// IPC backends and run them in-process.
//
// (See pepper_in_process_resource_creation.h for more background.)
//
// This class just provides the fake routing for in-process plugins.
// Asynchronous messages are converted into an asynchronous execution of the
// message receiver on the opposite end. Synchronous messages just call right
// through.
//
// The resources in ppapi/proxy assume that there is an IPC connection to
// both the renderer and the browser processes. They take a connection object
// that includes both of these channels. However, in-process plugins don't
// have a BrowserPpapiHost on the browser side to receive these messages.
//
// As a result, we can't support resources that rely on sending messages to the
// browser process. Since this class is a stopgap until all interfaces are
// converted and all plugins are run out-of-procss, we just choose not to
// support faking IPC channels for resources that send messages directly to the
// browser process. These resources will just have to use the "old" in-process
// implementation until the conversion is complete and all this code can be
// deleted.
//
// To keep things consistent, we provide an IPC::Sender for the browser channel
// in the connection object supplied to resources. This dummy browser channel
// will just assert and delete the message if anything is ever sent over it.
//
// There are two restrictions for in-process resource calls:
// Sync messages can only be sent from the plugin to the host.
// The host must handle sync messages synchronously.
class PepperInProcessRouter {
 public:
  // The given host parameter owns this class and must outlive us.
  PepperInProcessRouter(RendererPpapiHostImpl* host_impl);
  ~PepperInProcessRouter();

  // Returns the dummy sender for the cooresponding end of the in-process
  // emulated channel.
  IPC::Sender* GetPluginToRendererSender();
  IPC::Sender* GetRendererToPluginSender();

  // Returns a connection pair for use by a resource proxy. This includes
  // the plugin->renderer sender as well as a dummy sender to the browser
  // process. See the class comment above about the dummy sender.
  ppapi::proxy::Connection GetPluginConnection(PP_Instance instance);

  // Handles resource reply messages from the host.
  static bool OnPluginMsgReceived(const IPC::Message& msg);

 private:
  bool SendToHost(IPC::Message* msg);
  bool SendToPlugin(IPC::Message* msg);
  void DispatchHostMsg(IPC::Message* msg);
  void DispatchPluginMsg(IPC::Message* msg);
  bool SendToBrowser(IPC::Message* msg);

  RendererPpapiHostImpl* host_impl_;

  class Channel;
  std::unique_ptr<Channel> browser_channel_;

  // Renderer -> plugin channel.
  std::unique_ptr<Channel> host_to_plugin_router_;

  // Plugin -> renderer channel.
  std::unique_ptr<Channel> plugin_to_host_router_;

  // Pending sync message id.
  int pending_message_id_;

  // Reply deserializer of the pending sync message.
  std::unique_ptr<IPC::MessageReplyDeserializer> reply_deserializer_;

  // Reply result of the pending sync message.
  bool reply_result_;

  base::WeakPtrFactory<PepperInProcessRouter> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PepperInProcessRouter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_IN_PROCESS_ROUTER_H_
