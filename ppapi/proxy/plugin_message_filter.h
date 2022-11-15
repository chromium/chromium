// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_MESSAGE_FILTER_H_
#define PPAPI_PROXY_PLUGIN_MESSAGE_FILTER_H_

#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "ipc/ipc_sender.h"
#include "ipc/message_filter.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/proxy/resource_message_filter.h"

namespace ppapi {
namespace proxy {

class ResourceMessageReplyParams;
class ResourceReplyThreadRegistrar;

// Listens for messages on the I/O thread of the plugin and handles some of
// them to avoid needing to block on the plugin.
//
// There is one instance of this class for each renderer channel (same as for
// the PluginDispatchers).
class PPAPI_PROXY_EXPORT PluginMessageFilter : public IPC::MessageFilter,
                                               public IPC::Sender {
 public:
  // |seen_instance_ids| is a pointer to a set that will be used to uniquify
  // PP_Instances across all renderer channels. The same pointer should be
  // passed to each MessageFilter to ensure uniqueness, and the value should
  // outlive this class. It could be NULL if this filter is for a browser
  // channel.
  // |thread_registrar| is used to look up handling threads for resource
  // reply messages. It shouldn't be NULL.
  PluginMessageFilter(
      std::set<PP_Instance>* seen_instance_ids,
      scoped_refptr<ResourceReplyThreadRegistrar> thread_registrar);
  ~PluginMessageFilter() override;

  // MessageFilter implementation.
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* msg) override;

  void AddResourceMessageFilter(
      const scoped_refptr<ResourceMessageFilter>& filter);

  // Simulates an incoming resource reply that is handled on the calling thread.
  // For testing only.
  static void DispatchResourceReplyForTest(
      const ResourceMessageReplyParams& reply_params,
      const IPC::Message& nested_msg);

 private:
  void OnMsgReserveInstanceId(PP_Instance instance, bool* usable);
  void OnMsgResourceReply(const ResourceMessageReplyParams& reply_params,
                          const IPC::Message& nested_msg);

  // Dispatches the given resource reply to the appropriate resource in the
  // plugin process.
  static void DispatchResourceReply(
      const ResourceMessageReplyParams& reply_params,
      const IPC::Message& nested_msg);

  // All instance IDs ever queried by any renderer on this plugin. This is used
  // to make sure that new instance IDs are unique. This is a non-owning
  // pointer. It is managed by PluginDispatcher::PluginDelegate.
  std::set<PP_Instance>* seen_instance_ids_;

  scoped_refptr<ResourceReplyThreadRegistrar> resource_reply_thread_registrar_;

  std::vector<scoped_refptr<ResourceMessageFilter>> resource_filters_;

  // The IPC sender to the renderer. May be NULL if we're not currently
  // attached as a filter.
  IPC::Sender* sender_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_MESSAGE_FILTER_H_
