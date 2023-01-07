// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_CONNECTION_H_
#define PPAPI_PROXY_CONNECTION_H_

#include "ipc/ipc_message.h"
#include "ppapi/proxy/plugin_dispatcher.h"

namespace IPC {
class Sender;
}

namespace ppapi {
namespace proxy {

// This struct holds the channels that a resource uses to send message to the
// browser and renderer.
class Connection {
 public:
  Connection()
      : browser_sender_(nullptr),
        in_process_renderer_sender_(nullptr),
        in_process_(false),
        browser_sender_routing_id_(MSG_ROUTING_NONE) {}
  Connection(
      IPC::Sender* browser,
      scoped_refptr<PluginDispatcher::Sender> out_of_process_renderer_sender)
      : browser_sender_(browser),
        in_process_renderer_sender_(nullptr),
        out_of_process_renderer_sender_(out_of_process_renderer_sender),
        in_process_(false),
        browser_sender_routing_id_(MSG_ROUTING_NONE) {}
  Connection(IPC::Sender* browser,
             IPC::Sender* in_process_renderer_sender,
             int routing_id)
      : browser_sender_(browser),
        in_process_renderer_sender_(in_process_renderer_sender),
        in_process_(true),
        browser_sender_routing_id_(routing_id) {}

  IPC::Sender* GetRendererSender() {
    return in_process_ ? in_process_renderer_sender_
                       : out_of_process_renderer_sender_.get();
  }
  IPC::Sender* browser_sender() { return browser_sender_; }
  bool in_process() { return in_process_; }
  int browser_sender_routing_id() { return browser_sender_routing_id_; }

 private:
  IPC::Sender* browser_sender_;

  IPC::Sender* in_process_renderer_sender_;
  scoped_refptr<PluginDispatcher::Sender> out_of_process_renderer_sender_;

  bool in_process_;
  // We need to use a routing ID when a plugin is in-process, and messages are
  // sent back from the browser to the renderer. This is so that messages are
  // routed to the proper RenderFrameImpl.
  int browser_sender_routing_id_;
};

}  // namespace proxy
}  // namespace ppapi


#endif  // PPAPI_PROXY_CONNECTION_H_

