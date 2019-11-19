// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_

#include <map>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "content/browser/renderer_host/pepper/content_browser_pepper_host_factory.h"
#include "content/common/content_export.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/common/process_type.h"
#include "ipc/message_filter.h"
#include "ppapi/buildflags/buildflags.h"
#include "ppapi/host/ppapi_host.h"

#if !BUILDFLAG(ENABLE_PLUGINS)
#error "Plugins should be enabled"
#endif

namespace content {

class CONTENT_EXPORT BrowserPpapiHostImpl : public BrowserPpapiHost {
 public:
  class InstanceObserver {
   public:
    // Called when the plugin instance is throttled or unthrottled because of
    // the Plugin Power Saver feature. Invoked on the IO thread.
    virtual void OnThrottleStateChanged(bool is_throttled) = 0;

    // Called right before the instance is destroyed.
    virtual void OnHostDestroyed() = 0;
  };

  // The creator is responsible for calling set_plugin_process as soon as it is
  // known (we start the process asynchronously so it won't be known when this
   // object is created).
  // |external_plugin| signfies that this is a proxy created for an embedder's
  // plugin, i.e. using BrowserPpapiHost::CreateExternalPluginProcess.
  BrowserPpapiHostImpl(IPC::Sender* sender,
                       const ppapi::PpapiPermissions& permissions,
                       const std::string& plugin_name,
                       const base::FilePath& plugin_path,
                       const base::FilePath& profile_data_directory,
                       bool in_process,
                       bool external_plugin);
  ~BrowserPpapiHostImpl() override;

  // BrowserPpapiHost.
  ppapi::host::PpapiHost* GetPpapiHost() override;
  const base::Process& GetPluginProcess() override;
  bool IsValidInstance(PP_Instance instance) override;
  bool GetRenderFrameIDsForInstance(PP_Instance instance,
                                    int* render_process_id,
                                    int* render_frame_id) override;
  const std::string& GetPluginName() override;
  const base::FilePath& GetPluginPath() override;
  const base::FilePath& GetProfileDataDirectory() override;
  GURL GetDocumentURLForInstance(PP_Instance instance) override;
  GURL GetPluginURLForInstance(PP_Instance instance) override;

  // Whether the plugin context is secure. That is, it is served from a secure
  // origin and it is embedded within a hierarchy of secure frames. This value
  // comes from the renderer so should not be trusted. It is used for metrics.
  bool IsPotentiallySecurePluginContext(PP_Instance instance);

  void set_plugin_process(base::Process process) {
    plugin_process_ = std::move(process);
  }

  bool external_plugin() const { return external_plugin_; }

  // These two functions are notifications that an instance has been created
  // or destroyed. They allow us to maintain a mapping of PP_Instance to data
  // associated with the instance including view IDs in the browser process.
  void AddInstance(PP_Instance instance,
                   const PepperRendererInstanceData& renderer_instance_data);
  void DeleteInstance(PP_Instance instance);

  void AddInstanceObserver(PP_Instance instance, InstanceObserver* observer);
  void RemoveInstanceObserver(PP_Instance instance, InstanceObserver* observer);

  void OnThrottleStateChanged(PP_Instance instance, bool is_throttled);
  bool IsThrottled(PP_Instance instance) const;

  scoped_refptr<IPC::MessageFilter> message_filter() {
    return message_filter_;
  }

 private:
  friend class BrowserPpapiHostTest;

  // Implementing MessageFilter on BrowserPpapiHostImpl makes it ref-counted,
  // preventing us from returning these to embedders without holding a
  // reference. To avoid that, define a message filter object.
  class HostMessageFilter : public IPC::MessageFilter {
   public:
    HostMessageFilter(ppapi::host::PpapiHost* ppapi_host,
                      BrowserPpapiHostImpl* browser_ppapi_host_impl);

    // IPC::MessageFilter.
    bool OnMessageReceived(const IPC::Message& msg) override;

    void OnHostDestroyed();

   private:
    ~HostMessageFilter() override;

    void OnHostMsgLogInterfaceUsage(int hash) const;

    // Non owning pointers cleared in OnHostDestroyed()
    ppapi::host::PpapiHost* ppapi_host_;
    BrowserPpapiHostImpl* browser_ppapi_host_impl_;
  };

  struct InstanceData {
    InstanceData(const PepperRendererInstanceData& renderer_data);
    ~InstanceData();

    PepperRendererInstanceData renderer_data;
    bool is_throttled;

    base::ObserverList<InstanceObserver>::Unchecked observer_list;
  };

  std::unique_ptr<ppapi::host::PpapiHost> ppapi_host_;
  base::Process plugin_process_;
  std::string plugin_name_;
  base::FilePath plugin_path_;
  base::FilePath profile_data_directory_;

  // If true, this refers to a plugin running in the renderer process.
  bool in_process_;

  // If true, this is an external plugin, i.e. created by the embedder using
  // BrowserPpapiHost::CreateExternalPluginProcess.
  bool external_plugin_;

  // Tracks all PP_Instances in this plugin and associated data.
  std::unordered_map<PP_Instance, std::unique_ptr<InstanceData>> instance_map_;

  scoped_refptr<HostMessageFilter> message_filter_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPpapiHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_BROWSER_PPAPI_HOST_IMPL_H_
