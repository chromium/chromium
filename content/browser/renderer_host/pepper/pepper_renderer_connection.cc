// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_renderer_connection.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"
#include "content/browser/renderer_host/pepper/pepper_file_ref_host.h"
#include "content/browser/renderer_host/pepper/pepper_file_system_browser_host.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"

namespace content {

namespace {

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

  raw_ptr<BrowserPpapiHostImpl> host_;
  raw_ptr<BrowserMessageFilter> connection_;
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

class PepperRendererConnection::OpenChannelToPpapiPluginCallback
    : public PpapiPluginProcessHost::PluginClient {
 public:
  OpenChannelToPpapiPluginCallback(
      PepperRendererConnection* filter,
      mojom::PepperHost::OpenChannelToPepperPluginCallback callback)
      : callback_(std::move(callback)), filter_(filter) {}

  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {
    // base::kNullProcessHandle indicates that the channel will be used by the
    // browser itself. Make sure we never output that value here.
    if (filter_->PeerHandle() == base::kNullProcessHandle) {
      return;
    }
    *renderer_handle = filter_->PeerHandle();
    *renderer_id = filter_->render_process_id_;
  }

  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId plugin_pid,
                            int plugin_child_id) override {
    std::move(callback_).Run(mojo::MakeScopedHandle(channel_handle.mojo_handle),
                             plugin_pid, plugin_child_id);
    delete this;
  }

  bool Incognito() override { return filter_->incognito_; }

 private:
  mojom::PepperHost::OpenChannelToPepperPluginCallback callback_;
  scoped_refptr<PepperRendererConnection> filter_;
};

PepperRendererConnection::PepperRendererConnection(
    int render_process_id,
    PluginServiceImpl* plugin_service,
    BrowserContext* browser_context,
    StoragePartition* storage_partition)
    : BrowserMessageFilter(PpapiMsgStart),
      render_process_id_(render_process_id),
      incognito_(browser_context->IsOffTheRecord()),
      plugin_service_(plugin_service),
      profile_data_directory_(storage_partition->GetPath()) {
  // Only give the renderer permission for stable APIs.
  in_process_host_ = std::make_unique<BrowserPpapiHostImpl>(
      this, ppapi::PpapiPermissions(), "", base::FilePath(), base::FilePath(),
      true /* in_process */, false /* external_plugin */);
}

PepperRendererConnection::~PepperRendererConnection() {}

BrowserPpapiHostImpl* PepperRendererConnection::GetHostForChildProcess(
    int child_process_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

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

void PepperRendererConnection::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  if (IPC_MESSAGE_ID_CLASS(message.type()) == PpapiMsgStart) {
    *thread = content::BrowserThread::UI;
  }
}

bool PepperRendererConnection::OnMessageReceived(const IPC::Message& msg) {
  if (in_process_host_->GetPpapiHost()->OnMessageReceived(msg))
    return true;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PepperRendererConnection, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_CreateResourceHostsFromHost,
                        OnMsgCreateResourceHostsFromHost)
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
          resource_host = std::make_unique<PepperFileRefHost>(
              host, instance, params.pp_resource(), external_path);
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

void PepperRendererConnection::DidCreateInProcessInstance(
    int32_t instance,
    int32_t render_frame_id,
    const GURL& document_url,
    const GURL& plugin_url) {
  // We don't need to know if it's a privileged context for in-process plugins.
  // In process plugins are deprecated and the only in-process plugin that
  // exists is the "NaCl plugin" which will never need to know this.
  PepperRendererInstanceData instance_data{render_process_id_, render_frame_id,
                                           document_url, plugin_url,
                                           /*secure=*/false};

  // 'instance' is possibly invalid. The host must be careful not to trust it.
  in_process_host_->AddInstance(instance, instance_data);
}

void PepperRendererConnection::DidDeleteInProcessInstance(int32_t instance) {
  // 'instance' is possibly invalid. The host must be careful not to trust it.
  in_process_host_->DeleteInstance(instance);
}

void PepperRendererConnection::DidCreateOutOfProcessPepperInstance(
    int32_t plugin_child_id,
    int32_t pp_instance,
    bool is_external,
    int32_t render_frame_id,
    const GURL& document_url,
    const GURL& plugin_url,
    bool is_privileged_context,
    mojom::PepperHost::DidCreateOutOfProcessPepperInstanceCallback callback) {
  // It's important that we supply the render process ID ourselves based on the
  // channel the message arrived on. We use the
  //   PP_Instance -> (process id, frame id)
  // mapping to decide how to handle messages received from the (untrusted)
  // plugin. An exploited renderer must not be able to insert fake mappings
  // that may allow it access to other render processes.
  PepperRendererInstanceData instance_data{render_process_id_, render_frame_id,
                                           document_url, plugin_url,
                                           is_privileged_context};
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->AddInstance(pp_instance, instance_data);
  } else {
    PpapiPluginProcessHost::DidCreateOutOfProcessInstance(
        plugin_child_id, pp_instance, instance_data);
  }
  std::move(callback).Run();
}

void PepperRendererConnection::DidDeleteOutOfProcessPepperInstance(
    int32_t plugin_child_id,
    int32_t pp_instance,
    bool is_external) {
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->DeleteInstance(pp_instance);
  } else {
    PpapiPluginProcessHost::DidDeleteOutOfProcessInstance(plugin_child_id,
                                                          pp_instance);
  }
}

void PepperRendererConnection::OpenChannelToPepperPlugin(
    const url::Origin& embedder_origin,
    const base::FilePath& path,
    const std::optional<url::Origin>& origin_lock,
    mojom::PepperHost::OpenChannelToPepperPluginCallback callback) {
  // Enforce that the sender of the IPC (i.e. |render_process_id_|) is actually
  // allowed to host a frame with |embedder_origin|. Note that sandboxed frames
  // or PDFs cannot host plugins, so it's safe to use the stricter
  // CanAccessDataForOrigin() instead of HostsOrigin().
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, embedder_origin)) {
    bad_message::ReceivedBadMessage(
        this, bad_message::RFMF_INVALID_PLUGIN_EMBEDDER_ORIGIN);
    return;
  }

  plugin_service_->OpenChannelToPpapiPlugin(
      render_process_id_, path, profile_data_directory_, origin_lock,
      new OpenChannelToPpapiPluginCallback(this, std::move(callback)));
}

}  // namespace content
