// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/host_dispatcher_wrapper.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/pepper/pepper_browser_connection.h"
#include "content/renderer/pepper/pepper_hung_plugin_filter.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_proxy_channel_delegate_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "content/renderer/pepper/renderer_restrict_dispatch_group.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_plugin_container.h"

namespace content {

HostDispatcherWrapper::HostDispatcherWrapper(
    PluginModule* module,
    base::ProcessId peer_pid,
    int plugin_child_id,
    const ppapi::PpapiPermissions& perms,
    bool is_external)
    : module_(module),
      peer_pid_(peer_pid),
      plugin_child_id_(plugin_child_id),
      permissions_(perms),
      is_external_(is_external) {}

HostDispatcherWrapper::~HostDispatcherWrapper() {
  hung_plugin_filter_->HostDispatcherDestroyed();
}

bool HostDispatcherWrapper::Init(
    const IPC::ChannelHandle& channel_handle,
    PP_GetInterface_Func local_get_interface,
    const ppapi::Preferences& preferences,
    scoped_refptr<PepperHungPluginFilter> filter,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (!channel_handle.is_mojo_channel_handle())
    return false;

  dispatcher_delegate_ = std::make_unique<PepperProxyChannelDelegateImpl>();
  dispatcher_ = std::make_unique<ppapi::proxy::HostDispatcher>(
      module_->pp_module(), local_get_interface, permissions_);
  // The HungPluginFilter needs to know when we are blocked on a sync message
  // to the plugin. Note the filter outlives the dispatcher, so there is no
  // need to remove it as an observer.
  dispatcher_->AddSyncMessageStatusObserver(filter.get());
  // Guarantee the hung_plugin_filter_ outlives |dispatcher_|.
  hung_plugin_filter_ = filter;

  if (!dispatcher_->InitHostWithChannel(dispatcher_delegate_.get(), peer_pid_,
                                        channel_handle,
                                        true,  // Client.
                                        preferences, task_runner)) {
    dispatcher_.reset();
    dispatcher_delegate_.reset();
    return false;
  }
  // HungPluginFilter needs to listen for some messages on the IO thread.
  dispatcher_->AddIOThreadMessageFilter(filter);

  dispatcher_->channel()->SetRestrictDispatchChannelGroup(
      kRendererRestrictDispatchGroup_Pepper);
  return true;
}

const void* HostDispatcherWrapper::GetProxiedInterface(const char* name) {
  return dispatcher_->GetProxiedInterface(name);
}

void HostDispatcherWrapper::AddInstance(PP_Instance instance) {
  ppapi::proxy::HostDispatcher::SetForInstance(instance, dispatcher_.get());

  RendererPpapiHostImpl* host =
      RendererPpapiHostImpl::GetForPPInstance(instance);
  // TODO(brettw) remove this null check when the old-style pepper-based
  // browser tag is removed from this file. Getting this notification should
  // always give us an instance we can find in the map otherwise, but that
  // isn't true for browser tag support.
  if (host) {
    RenderFrame* render_frame = host->GetRenderFrameForInstance(instance);
    PepperPluginInstance* plugin_instance = host->GetPluginInstance(instance);
    bool is_privileged_context =
        plugin_instance->GetContainer()->GetDocument().IsSecureContext() &&
        network::IsUrlPotentiallyTrustworthy(plugin_instance->GetPluginURL());
    PepperBrowserConnection::Get(render_frame)
        ->DidCreateOutOfProcessPepperInstance(
            plugin_child_id_, instance, is_external_,
            render_frame->GetRoutingID(), host->GetDocumentURL(instance),
            plugin_instance->GetPluginURL(), is_privileged_context);
  }
}

void HostDispatcherWrapper::RemoveInstance(PP_Instance instance) {
  ppapi::proxy::HostDispatcher::RemoveForInstance(instance);

  RendererPpapiHostImpl* host =
      RendererPpapiHostImpl::GetForPPInstance(instance);
  // TODO(brettw) remove null check as described in AddInstance.
  if (host) {
    RenderFrame* render_frame = host->GetRenderFrameForInstance(instance);
    if (render_frame) {
      PepperBrowserConnection::Get(render_frame)
          ->DidDeleteOutOfProcessPepperInstance(plugin_child_id_, instance,
                                                is_external_);
    }
  }
}

}  // namespace content
