// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"
#include "content/common/frame_messages.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

namespace {

const uint32_t kPepperFilteredMessageClasses[] = {
    PpapiMsgStart, FrameMsgStart,
};

// Responsible for creating the pending resource hosts, holding their IDs until
// all of them have been created for a single message, and sending the reply to
// say that the hosts have been created.
class PendingHostCreator : public base::RefCounted<PendingHostCreator> {
 public:
  PendingHostCreator(BrowserPpapiHostImpl* host,
                     BrowserMessageFilter* connection,
                     int routing_id,
                     int sequence_id,
                     size_t nested_msgs_size);

  // Adds the given resource host as a pending one. The host is remembered as
  // host number |index|, and will ultimately be sent to the plugin to be
  // attached to a real resource.
  void AddPendingResourceHost(
      size_t index,
      std::unique_ptr<ppapi::host::ResourceHost> resource_host);

 private:
  friend class base::RefCounted<PendingHostCreator>;

  // When the last reference to this class is released, all of the resource
  // hosts would have been added. This destructor sends the message to the
  // plugin to tell it to attach real hosts to all of the pending hosts that
  // have been added by this object.
  ~PendingHostCreator();

  BrowserPpapiHostImpl* host_;
  BrowserMessageFilter* connection_;
  int routing_id_;
  int sequence_id_;
  std::vector<int> pending_resource_host_ids_;
};

PendingHostCreator::PendingHostCreator(BrowserPpapiHostImpl* host,
                                       BrowserMessageFilter* connection,
                                       int routing_id,
                                       int sequence_id,
                                       size_t nested_msgs_size)
    : host_(host),
      connection_(connection),
      routing_id_(routing_id),
      sequence_id_(sequence_id),
      pending_resource_host_ids_(nested_msgs_size, 0) {}

void PendingHostCreator::AddPendingResourceHost(
    size_t index,
    std::unique_ptr<ppapi::host::ResourceHost> resource_host) {
  pending_resource_host_ids_[index] =
      host_->GetPpapiHost()->AddPendingResourceHost(std::move(resource_host));
}

PendingHostCreator::~PendingHostCreator() {
  connection_->Send(new PpapiHostMsg_CreateResourceHostsFromHostReply(
      routing_id_, sequence_id_, pending_resource_host_ids_));
}

}  // namespace

PepperRendererConnection::PepperRendererConnection(int render_process_id)
    : BrowserMessageFilter(kPepperFilteredMessageClasses,
                           base::size(kPepperFilteredMessageClasses)),
      render_process_id_(render_process_id) {
  // Only give the renderer permission for stable APIs.
  in_process_host_.reset(new BrowserPpapiHostImpl(this,
                                                  ppapi::PpapiPermissions(),
                                                  "",
                                                  base::FilePath(),
                                                  base::FilePath(),
                                                  true /* in_process */,
                                                  false /* external_plugin */));
}

PepperRendererConnection::~PepperRendererConnection() {}

BrowserPpapiHostImpl* PepperRendererConnection::GetHostForChildProcess(
    int child_process_id) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Find the plugin which this message refers to. Check NaCl plugins first.
  BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
      GetContentClient()->browser()->GetExternalBrowserPpapiHost(
          child_process_id));

  if (!host) {
    // Check trusted pepper plugins.
    for (PpapiPluginProcessHostIterator iter; !iter.Done(); ++iter) {
      if (iter->process() &&
          iter->process()->GetData().id == child_process_id) {
        // Found the plugin.
        host = iter->host_impl();
        break;
      }
    }
  }

  // If the message is being sent from an in-process plugin, we own the
  // BrowserPpapiHost.
  if (!host && child_process_id == 0) {
    host = in_process_host_.get();
  }

  return host;
}

bool PepperRendererConnection::OnMessageReceived(const IPC::Message& msg) {
  if (in_process_host_->GetPpapiHost()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PepperRendererConnection, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_CreateResourceHostsFromHost,
                        OnMsgCreateResourceHostsFromHost)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidCreateInProcessInstance,
                        OnMsgDidCreateInProcessInstance)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidDeleteInProcessInstance,
                        OnMsgDidDeleteInProcessInstance)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void PepperRendererConnection::OnMsgCreateResourceHostsFromHost(
    int routing_id,
    int child_process_id,
    const ppapi::proxy::ResourceMessageCallParams& params,
    PP_Instance instance,
    const std::vector<IPC::Message>& nested_msgs) {
  BrowserPpapiHostImpl* host = GetHostForChildProcess(child_process_id);
  if (!host) {
    DLOG(ERROR) << "Invalid plugin process ID.";
    return;
  }

  scoped_refptr<PendingHostCreator> creator = new PendingHostCreator(
      host, this, routing_id, params.sequence(), nested_msgs.size());
  for (size_t i = 0; i < nested_msgs.size(); ++i) {
    const IPC::Message& nested_msg = nested_msgs[i];
    std::unique_ptr<ppapi::host::ResourceHost> resource_host;
    if (host->IsValidInstance(instance)) {
      if (nested_msg.type() == PpapiHostMsg_FileRef_CreateForRawFS::ID) {
        // FileRef_CreateForRawFS is only permitted from the renderer. Because
        // of this, we handle this message here and not in
        // content_browser_pepper_host_factory.cc.
        base::FilePath external_path;
        if (ppapi::UnpackMessage<PpapiHostMsg_FileRef_CreateForRawFS>(
                nested_msg, &external_path)) {
          resource_host.reset(new PepperFileRefHost(
              host, instance, params.pp_resource(), external_path));
        }
      } else if (nested_msg.type() ==
                 PpapiHostMsg_FileSystem_CreateFromRenderer::ID) {
        // Similarly, FileSystem_CreateFromRenderer is only permitted from the
        // renderer.
        std::string root_url;
        PP_FileSystemType file_system_type;
        if (ppapi::UnpackMessage<PpapiHostMsg_FileSystem_CreateFromRenderer>(
                nested_msg, &root_url, &file_system_type)) {
          PepperFileSystemBrowserHost* browser_host =
              new PepperFileSystemBrowserHost(
                  host, instance, params.pp_resource(), file_system_type);
          resource_host.reset(browser_host);
          // Open the file system resource host. This is an asynchronous
          // operation, and we must only add the pending resource host and
          // send the message once it completes.
          browser_host->OpenExisting(
              GURL(root_url),
              base::BindOnce(&PendingHostCreator::AddPendingResourceHost,
                             creator, i, std::move(resource_host)));
          // Do not fall through; the fall-through case adds the pending
          // resource host to the list. We must do this asynchronously.
          continue;
        }
      }
    }

    if (!resource_host.get()) {
      resource_host = host->GetPpapiHost()->CreateResourceHost(
          params.pp_resource(), instance, nested_msg);
    }

    if (resource_host.get())
      creator->AddPendingResourceHost(i, std::move(resource_host));
  }

  // Note: All of the pending host IDs that were added as part of this
  // operation will automatically be sent to the plugin when |creator| is
  // released. This may happen immediately, or (if there are asynchronous
  // requests to create resource hosts), once all of them complete.
}

void PepperRendererConnection::OnMsgDidCreateInProcessInstance(
    PP_Instance instance,
    const PepperRendererInstanceData& instance_data) {
  PepperRendererInstanceData data = instance_data;
  // It's important that we supply the render process ID ourselves since the
  // message may be coming from a compromised renderer.
  data.render_process_id = render_process_id_;
  // 'instance' is possibly invalid. The host must be careful not to trust it.
  in_process_host_->AddInstance(instance, data);
}

void PepperRendererConnection::OnMsgDidDeleteInProcessInstance(
    PP_Instance instance) {
  // 'instance' is possibly invalid. The host must be careful not to trust it.
  in_process_host_->DeleteInstance(instance);
}

}  // namespace content
