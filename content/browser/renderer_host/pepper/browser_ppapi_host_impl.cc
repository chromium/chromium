// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"

#include "base/metrics/histogram_functions.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

// static
BrowserPpapiHost* BrowserPpapiHost::CreateExternalPluginProcess(
    IPC::Sender* sender,
    ppapi::PpapiPermissions permissions,
    base::ProcessHandle plugin_child_process,
    IPC::ChannelProxy* channel,
    int render_process_id,
    int render_view_id,
    const base::FilePath& profile_directory) {
  // The plugin name and path shouldn't be needed for external plugins.
  BrowserPpapiHostImpl* browser_ppapi_host =
      new BrowserPpapiHostImpl(sender,
                               permissions,
                               std::string(),
                               base::FilePath(),
                               profile_directory,
                               false /* in_process */,
                               true /* external_plugin */);
  browser_ppapi_host->set_plugin_process(
      base::Process::DeprecatedGetProcessFromHandle(plugin_child_process));

  scoped_refptr<PepperMessageFilter> pepper_message_filter(
      new PepperMessageFilter());
  channel->AddFilter(pepper_message_filter->GetFilter());
  channel->AddFilter(browser_ppapi_host->message_filter().get());

  return browser_ppapi_host;
}

BrowserPpapiHostImpl::BrowserPpapiHostImpl(
    IPC::Sender* sender,
    const ppapi::PpapiPermissions& permissions,
    const std::string& plugin_name,
    const base::FilePath& plugin_path,
    const base::FilePath& profile_data_directory,
    bool in_process,
    bool external_plugin)
    : ppapi_host_(new ppapi::host::PpapiHost(sender, permissions)),
      plugin_name_(plugin_name),
      plugin_path_(plugin_path),
      profile_data_directory_(profile_data_directory),
      in_process_(in_process),
      external_plugin_(external_plugin) {
  message_filter_ = new HostMessageFilter(ppapi_host_.get(), this);
  ppapi_host_->AddHostFactoryFilter(std::unique_ptr<ppapi::host::HostFactory>(
      new ContentBrowserPepperHostFactory(this)));
}

BrowserPpapiHostImpl::~BrowserPpapiHostImpl() {
  // Notify the filter so it won't foward messages to us.
  message_filter_->OnHostDestroyed();

  // Notify instance observers about our impending destruction.
  for (auto& instance_data : instance_map_) {
    for (auto& observer : instance_data.second->observer_list)
      observer.OnHostDestroyed();
  }

  // Delete the host explicitly first. This shutdown will destroy the
  // resources, which may want to do cleanup in their destructors and expect
  // their pointers to us to be valid.
  ppapi_host_.reset();
}

ppapi::host::PpapiHost* BrowserPpapiHostImpl::GetPpapiHost() {
  return ppapi_host_.get();
}

const base::Process& BrowserPpapiHostImpl::GetPluginProcess() {
  // Handle should previously have been set before use.
  DCHECK(in_process_ || plugin_process_.IsValid());
  return plugin_process_;
}

bool BrowserPpapiHostImpl::IsValidInstance(PP_Instance instance) {
  return instance_map_.find(instance) != instance_map_.end();
}

bool BrowserPpapiHostImpl::GetRenderFrameIDsForInstance(PP_Instance instance,
                                                        int* render_process_id,
                                                        int* render_frame_id) {
  auto it = instance_map_.find(instance);
  if (it == instance_map_.end()) {
    *render_process_id = 0;
    *render_frame_id = 0;
    return false;
  }

  *render_process_id = it->second->renderer_data.render_process_id;
  *render_frame_id = it->second->renderer_data.render_frame_id;
  return true;
}

const std::string& BrowserPpapiHostImpl::GetPluginName() {
  return plugin_name_;
}

const base::FilePath& BrowserPpapiHostImpl::GetPluginPath() {
  return plugin_path_;
}

const base::FilePath& BrowserPpapiHostImpl::GetProfileDataDirectory() {
  return profile_data_directory_;
}

GURL BrowserPpapiHostImpl::GetDocumentURLForInstance(PP_Instance instance) {
  auto it = instance_map_.find(instance);
  if (it == instance_map_.end())
    return GURL();
  return it->second->renderer_data.document_url;
}

GURL BrowserPpapiHostImpl::GetPluginURLForInstance(PP_Instance instance) {
  auto it = instance_map_.find(instance);
  if (it == instance_map_.end())
    return GURL();
  return it->second->renderer_data.plugin_url;
}

bool BrowserPpapiHostImpl::IsPotentiallySecurePluginContext(
    PP_Instance instance) {
  auto it = instance_map_.find(instance);
  if (it == instance_map_.end())
    return false;
  return it->second->renderer_data.is_potentially_secure_plugin_context;
}

void BrowserPpapiHostImpl::AddInstance(
    PP_Instance instance,
    const PepperRendererInstanceData& renderer_instance_data) {
  // NOTE: 'instance' may be coming from a compromised renderer process. We
  // take care here to make sure an attacker can't overwrite data for an
  // existing plugin instance.
  // See http://crbug.com/733548.
  if (instance_map_.find(instance) == instance_map_.end()) {
    instance_map_[instance] =
        std::make_unique<InstanceData>(renderer_instance_data);
  } else {
    NOTREACHED();
  }
}

void BrowserPpapiHostImpl::DeleteInstance(PP_Instance instance) {
  // NOTE: 'instance' may be coming from a compromised renderer process. We
  // take care here to make sure an attacker can't cause a UAF by deleting a
  // non-existent plugin instance.
  // See http://crbug.com/733548.
  auto it = instance_map_.find(instance);
  if (it != instance_map_.end()) {
    // We need to tell the observers for that instance that we are destroyed
    // because we won't have the opportunity to once we remove them from the
    // |instance_map_|. If the instance was deleted, observers for those
    // instances should never call back into the host anyway, so it is safe to
    // tell them that the host is destroyed.
    for (auto& observer : it->second->observer_list)
      observer.OnHostDestroyed();

    instance_map_.erase(it);
  } else {
    NOTREACHED();
  }
}

void BrowserPpapiHostImpl::AddInstanceObserver(PP_Instance instance,
                                               InstanceObserver* observer) {
  instance_map_[instance]->observer_list.AddObserver(observer);
}

void BrowserPpapiHostImpl::RemoveInstanceObserver(PP_Instance instance,
                                                  InstanceObserver* observer) {
  auto it = instance_map_.find(instance);
  if (it != instance_map_.end())
    it->second->observer_list.RemoveObserver(observer);
}

void BrowserPpapiHostImpl::OnThrottleStateChanged(PP_Instance instance,
                                                  bool is_throttled) {
  auto it = instance_map_.find(instance);
  if (it != instance_map_.end()) {
    it->second->is_throttled = is_throttled;
    for (auto& observer : it->second->observer_list)
      observer.OnThrottleStateChanged(is_throttled);
  }
}

bool BrowserPpapiHostImpl::IsThrottled(PP_Instance instance) const {
  auto it = instance_map_.find(instance);
  if (it != instance_map_.end())
    return it->second->is_throttled;

  return false;
}

BrowserPpapiHostImpl::HostMessageFilter::HostMessageFilter(
    ppapi::host::PpapiHost* ppapi_host,
    BrowserPpapiHostImpl* browser_ppapi_host_impl)
    : ppapi_host_(ppapi_host),
      browser_ppapi_host_impl_(browser_ppapi_host_impl) {}

bool BrowserPpapiHostImpl::HostMessageFilter::OnMessageReceived(
    const IPC::Message& msg) {
  // Don't forward messages if our owner object has been destroyed.
  if (!ppapi_host_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPpapiHostImpl::HostMessageFilter, msg)
  // Add necessary message handlers here.
  IPC_MESSAGE_HANDLER(PpapiHostMsg_LogInterfaceUsage,
                      OnHostMsgLogInterfaceUsage)
  IPC_MESSAGE_UNHANDLED(handled = ppapi_host_->OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPpapiHostImpl::HostMessageFilter::OnHostDestroyed() {
  DCHECK(ppapi_host_);
  ppapi_host_ = nullptr;
  browser_ppapi_host_impl_ = nullptr;
}

BrowserPpapiHostImpl::HostMessageFilter::~HostMessageFilter() {}

void BrowserPpapiHostImpl::HostMessageFilter::OnHostMsgLogInterfaceUsage(
    int hash) const {
  base::UmaHistogramSparse("Pepper.InterfaceUsed", hash);
}

BrowserPpapiHostImpl::InstanceData::InstanceData(
    const PepperRendererInstanceData& renderer_data)
    : renderer_data(renderer_data), is_throttled(false) {
}

BrowserPpapiHostImpl::InstanceData::~InstanceData() {
}

}  // namespace content
