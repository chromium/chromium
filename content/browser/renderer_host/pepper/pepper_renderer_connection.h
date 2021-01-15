// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_RENDERER_CONNECTION_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_RENDERER_CONNECTION_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/pepper_plugin.mojom.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

namespace ppapi {
namespace proxy {
class ResourceMessageCallParams;
}
}

namespace content {

class BrowserContext;
class BrowserPpapiHostImpl;
class PluginServiceImpl;
class StoragePartition;

// This class represents a connection from the browser to the renderer for
// sending/receiving pepper ResourceHost related messages. When the browser
// and renderer communicate about ResourceHosts, they should pass the plugin
// process ID to identify which plugin they are talking about.
class PepperRendererConnection
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<mojom::PepperIOHost> {
 public:
  PepperRendererConnection(int render_process_id,
                           PluginServiceImpl* plugin_service,
                           BrowserContext* browser_context,
                           StoragePartition* storage_partition);

  // BrowserMessageFilter overrides.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  ~PepperRendererConnection() override;

  class OpenChannelToPpapiPluginCallback;
  // Returns the host for the child process for the given |child_process_id|.
  // If |child_process_id| is 0, returns the host owned by this
  // PepperRendererConnection, which serves as the host for in-process plugins.
  BrowserPpapiHostImpl* GetHostForChildProcess(int child_process_id) const;

  void OnMsgCreateResourceHostsFromHost(
      int routing_id,
      int child_process_id,
      const ppapi::proxy::ResourceMessageCallParams& params,
      PP_Instance instance,
      const std::vector<IPC::Message>& nested_msgs);

  // mojom::PepperPluginInstanceIOHost overrides;
  void DidCreateInProcessInstance(int32_t instance,
                                  int32_t render_frame_id,
                                  const GURL& document_url,
                                  const GURL& plugin_url) override;
  void DidDeleteInProcessInstance(int32_t instance) override;
  void DidCreateOutOfProcessPepperInstance(
      int32_t plugin_child_id,
      int32_t pp_instance,
      bool is_external,
      int32_t render_frame_id,
      const GURL& document_url,
      const GURL& plugin_url,
      bool is_priviledged_context,
      DidCreateOutOfProcessPepperInstanceCallback callback) override;
  void DidDeleteOutOfProcessPepperInstance(int32_t plugin_child_id,
                                           int32_t pp_instance,
                                           bool is_external) override;
  void OpenChannelToPepperPlugin(
      const url::Origin& embedder_origin,
      const base::FilePath& path,
      const base::Optional<url::Origin>& origin_lock,
      OpenChannelToPepperPluginCallback callback) override;

  const int render_process_id_;
  const bool incognito_;

  // We have a single BrowserPpapiHost per-renderer for all in-process plugins
  // running. This is just a work-around allowing new style resources to work
  // with the browser when running in-process but it means that plugin-specific
  // information (like the plugin name) won't be available.
  std::unique_ptr<BrowserPpapiHostImpl> in_process_host_;

  PluginServiceImpl* const plugin_service_;
  const base::FilePath profile_data_directory_;

  DISALLOW_COPY_AND_ASSIGN(PepperRendererConnection);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_RENDERER_CONNECTION_H_
