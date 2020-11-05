// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_frame_message_filter.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/syslog_logging.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/ipc_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/resource_context_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/mojom/frame/frame_owner_element_type.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !defined(OS_MAC)
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/public/browser/plugin_service_filter.h"
#endif

namespace content {

namespace {

// Common functionality for converting a sync renderer message to a callback
// function in the browser. Derive from this, create it on the heap when
// issuing your callback. When done, write your reply parameters into
// reply_msg(), and then call SendReplyAndDeleteThis().
class RenderMessageCompletionCallback {
 public:
  RenderMessageCompletionCallback(RenderFrameMessageFilter* filter,
                                  IPC::Message* reply_msg)
      : filter_(filter), reply_msg_(reply_msg) {}

  virtual ~RenderMessageCompletionCallback() {
    if (reply_msg_) {
      // If the owner of this class failed to call SendReplyAndDeleteThis(),
      // send an error reply to prevent the renderer from being hung.
      reply_msg_->set_reply_error();
      filter_->Send(reply_msg_);
    }
  }

  RenderFrameMessageFilter* filter() { return filter_.get(); }
  IPC::Message* reply_msg() { return reply_msg_; }

  void SendReplyAndDeleteThis() {
    filter_->Send(reply_msg_);
    reply_msg_ = nullptr;
    delete this;
  }

 private:
  scoped_refptr<RenderFrameMessageFilter> filter_;
  IPC::Message* reply_msg_;
};

}  // namespace

#if BUILDFLAG(ENABLE_PLUGINS)

class RenderFrameMessageFilter::OpenChannelToPpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PpapiPluginProcessHost::PluginClient {
 public:
  OpenChannelToPpapiPluginCallback(RenderFrameMessageFilter* filter,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg) {}

  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {
    // base::kNullProcessHandle indicates that the channel will be used by the
    // browser itself. Make sure we never output that value here.
    CHECK_NE(base::kNullProcessHandle, filter()->PeerHandle());
    *renderer_handle = filter()->PeerHandle();
    *renderer_id = filter()->render_process_id_;
  }

  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId plugin_pid,
                            int plugin_child_id) override {
    FrameHostMsg_OpenChannelToPepperPlugin::WriteReplyParams(
        reply_msg(), channel_handle, plugin_pid, plugin_child_id);
    SendReplyAndDeleteThis();
  }

  bool Incognito() override { return filter()->incognito_; }
};

#endif  // ENABLE_PLUGINS

RenderFrameMessageFilter::RenderFrameMessageFilter(
    int render_process_id,
    PluginServiceImpl* plugin_service,
    BrowserContext* browser_context,
    StoragePartition* storage_partition,
    RenderWidgetHelper* render_widget_helper)
    : BrowserMessageFilter(FrameMsgStart),
#if BUILDFLAG(ENABLE_PLUGINS)
      plugin_service_(plugin_service),
      profile_data_directory_(storage_partition->GetPath()),
#endif  // ENABLE_PLUGINS
      resource_context_(browser_context->GetResourceContext()),
      render_widget_helper_(render_widget_helper),
      incognito_(browser_context->IsOffTheRecord()),
      render_process_id_(render_process_id) {
}

RenderFrameMessageFilter::~RenderFrameMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void RenderFrameMessageFilter::ClearResourceContext() {
  resource_context_ = nullptr;
}

bool RenderFrameMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameMessageFilter, message)
#if BUILDFLAG(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER(FrameHostMsg_GetPluginInfo, OnGetPluginInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_OpenChannelToPepperPlugin,
                                    OnOpenChannelToPepperPlugin)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidCreateOutOfProcessPepperInstance,
                        OnDidCreateOutOfProcessPepperInstance)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidDeleteOutOfProcessPepperInstance,
                        OnDidDeleteOutOfProcessPepperInstance)
#endif  // ENABLE_PLUGINS
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderFrameMessageFilter::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void RenderFrameMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
#if BUILDFLAG(ENABLE_PLUGINS)
  if (message.type() == FrameHostMsg_GetPluginInfo::ID)
    *thread = BrowserThread::UI;
#endif  // ENABLE_PLUGINS
}

#if BUILDFLAG(ENABLE_PLUGINS)

void RenderFrameMessageFilter::OnGetPluginInfo(
    int render_frame_id,
    const GURL& url,
    const url::Origin& main_frame_origin,
    const std::string& mime_type,
    bool* found,
    WebPluginInfo* info,
    std::string* actual_mime_type) {
  bool allow_wildcard = true;
  *found = plugin_service_->GetPluginInfo(
      render_process_id_, render_frame_id, url, main_frame_origin, mime_type,
      allow_wildcard, nullptr, info, actual_mime_type);
}

void RenderFrameMessageFilter::OnOpenChannelToPepperPlugin(
    const url::Origin& embedder_origin,
    const base::FilePath& path,
    const base::Optional<url::Origin>& origin_lock,
    IPC::Message* reply_msg) {
  // Enforce that the sender of the IPC (i.e. |render_process_id_|) is actually
  // able/allowed to host a frame with |embedder_origin|.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, embedder_origin)) {
    NOTREACHED() << embedder_origin;
    bad_message::ReceivedBadMessage(
        this, bad_message::RFMF_INVALID_PLUGIN_EMBEDDER_ORIGIN);
    return;
  }

  plugin_service_->OpenChannelToPpapiPlugin(
      render_process_id_, embedder_origin, path, profile_data_directory_,
      origin_lock, new OpenChannelToPpapiPluginCallback(this, reply_msg));
}

void RenderFrameMessageFilter::OnDidCreateOutOfProcessPepperInstance(
    int plugin_child_id,
    int32_t pp_instance,
    PepperRendererInstanceData instance_data,
    bool is_external) {
  // It's important that we supply the render process ID ourselves based on the
  // channel the message arrived on. We use the
  //   PP_Instance -> (process id, frame id)
  // mapping to decide how to handle messages received from the (untrusted)
  // plugin, so an exploited renderer must not be able to insert fake mappings
  // that may allow it access to other render processes.
  DCHECK_EQ(0, instance_data.render_process_id);
  instance_data.render_process_id = render_process_id_;
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
}

void RenderFrameMessageFilter::OnDidDeleteOutOfProcessPepperInstance(
    int plugin_child_id,
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

#endif  // ENABLE_PLUGINS

}  // namespace content
