// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_PPAPI_HOST_H_
#define PPAPI_HOST_PPAPI_HOST_H_

#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/ppapi_host_export.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace ppapi {

namespace proxy {
class ResourceMessageCallParams;
class SerializedHandle;
}

namespace host {

class HostFactory;
struct HostMessageContext;
class InstanceMessageFilter;
struct ReplyMessageContext;
class ResourceHost;

// The host provides routing and tracking for resource message calls that
// come from the plugin to the host (browser or renderer), and the
// corresponding replies.
class PPAPI_HOST_EXPORT PpapiHost : public IPC::Sender, public IPC::Listener {
 public:
  // The sender is the channel to the plugin for outgoing messages.
  // Normally the creator will add filters for resource creation messages
  // (AddHostFactoryFilter) and instance messages (AddInstanceMessageFilter)
  // after construction.
  PpapiHost(IPC::Sender* sender, const PpapiPermissions& perms);

  PpapiHost(const PpapiHost&) = delete;
  PpapiHost& operator=(const PpapiHost&) = delete;

  ~PpapiHost() override;

  const PpapiPermissions& permissions() const { return permissions_; }

  // Sender implementation. Forwards to the sender_.
  bool Send(IPC::Message* msg) override;

  // Listener implementation.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Sends the given reply message to the plugin.
  void SendReply(const ReplyMessageContext& context,
                 const IPC::Message& msg);

  // Sends the given unsolicited reply message to the plugin.
  void SendUnsolicitedReply(PP_Resource resource, const IPC::Message& msg);

  // Similar to |SendUnsolicitedReply()|, but also sends handles.
  void SendUnsolicitedReplyWithHandles(
      PP_Resource resource,
      const IPC::Message& msg,
      std::vector<proxy::SerializedHandle>* handles);

  // Create a ResourceHost with the given |nested_msg|.
  std::unique_ptr<ResourceHost> CreateResourceHost(
      PP_Resource resource,
      PP_Instance instance,
      const IPC::Message& nested_msg);

  // Adds the given host resource as a pending one (with no corresponding
  // PluginResource object and no PP_Resource ID yet). The pending resource ID
  // is returned. See PpapiHostMsg_AttachToPendingHost.
  int AddPendingResourceHost(std::unique_ptr<ResourceHost> resource_host);

  // Adds the given host factory filter to the host. The PpapiHost will take
  // ownership of the pointer.
  void AddHostFactoryFilter(std::unique_ptr<HostFactory> filter);

  // Adds the given message filter to the host. The PpapiHost will take
  // ownership of the pointer.
  void AddInstanceMessageFilter(std::unique_ptr<InstanceMessageFilter> filter);

  // Returns null if the resource doesn't exist.
  host::ResourceHost* GetResourceHost(PP_Resource resource) const;

 private:
  friend class InstanceMessageFilter;

  void HandleResourceCall(
      const proxy::ResourceMessageCallParams& params,
      const IPC::Message& nested_msg,
      HostMessageContext* context);

  // Message handlers.
  void OnHostMsgResourceCall(const proxy::ResourceMessageCallParams& params,
                             const IPC::Message& nested_msg);
  void OnHostMsgInProcessResourceCall(
      int routing_id,
      const proxy::ResourceMessageCallParams& params,
      const IPC::Message& nested_msg);
  void OnHostMsgResourceSyncCall(
      const proxy::ResourceMessageCallParams& params,
      const IPC::Message& nested_msg,
      IPC::Message* reply_msg);
  void OnHostMsgResourceCreated(const proxy::ResourceMessageCallParams& param,
                                PP_Instance instance,
                                const IPC::Message& nested_msg);
  void OnHostMsgAttachToPendingHost(PP_Resource resource, int pending_host_id);
  void OnHostMsgResourceDestroyed(PP_Resource resource);

  // Non-owning pointer.
  IPC::Sender* sender_;

  PpapiPermissions permissions_;

  // Filters for resource creation messages. Note that since we don't support
  // deleting these dynamically we don't need to worry about modifications
  // during iteration. If we add that capability, this should be replaced with
  // an base::ObserverList.
  std::vector<std::unique_ptr<HostFactory>> host_factory_filters_;

  // Filters for instance messages. Note that since we don't support deleting
  // these dynamically we don't need to worry about modifications during
  // iteration. If we add that capability, this should be replaced with an
  // base::ObserverList.
  std::vector<std::unique_ptr<InstanceMessageFilter>> instance_message_filters_;

  typedef std::map<PP_Resource, std::unique_ptr<ResourceHost>> ResourceMap;
  ResourceMap resources_;

  // Resources that have been created in the host and have not yet had the
  // corresponding PluginResource associated with them.
  // See PpapiHostMsg_AttachToPendingHost.
  typedef std::map<int, std::unique_ptr<ResourceHost>> PendingHostResourceMap;
  PendingHostResourceMap pending_resource_hosts_;
  int next_pending_resource_host_id_;
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_PPAPI_HOST_H_
