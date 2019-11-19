// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_MESSAGE_FILTER_H_

#include <stdint.h>

#include <set>
#include <string>

#include "base/optional.h"
#include "content/common/frame_replication_state.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/three_d_api_types.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/common/pepper_renderer_instance_data.h"
#endif

struct FrameHostMsg_CreateChildFrame_Params;
struct FrameHostMsg_CreateChildFrame_Params_Reply;
class GURL;

namespace url {
class Origin;
}

namespace content {
class BrowserContext;
class PluginServiceImpl;
class RenderWidgetHelper;
class ResourceContext;
class StoragePartition;
struct WebPluginInfo;

// RenderFrameMessageFilter intercepts FrameHost messages on the IO thread
// that require low-latency processing. The canonical example of this is
// child-frame creation which is a sync IPC that provides the renderer
// with the routing id for a newly created RenderFrame.
//
// This object is created on the UI thread and used on the IO thread.
class CONTENT_EXPORT RenderFrameMessageFilter : public BrowserMessageFilter {
 public:
  RenderFrameMessageFilter(int render_process_id,
                           PluginServiceImpl* plugin_service,
                           BrowserContext* browser_context,
                           StoragePartition* storage_partition,
                           RenderWidgetHelper* render_widget_helper);

  // BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                BrowserThread::ID* thread) override;

  // Clears |resource_context_| to prevent accessing it after deletion.
  void ClearResourceContext();

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<RenderFrameMessageFilter>;

  class OpenChannelToPpapiPluginCallback;
  class OpenChannelToPpapiBrokerCallback;

  ~RenderFrameMessageFilter() override;

  // |params_reply| is an out parameter. Browser process defines it for the
  // renderer process.
  void OnCreateChildFrame(
      const FrameHostMsg_CreateChildFrame_Params& params,
      FrameHostMsg_CreateChildFrame_Params_Reply* params_reply);


  void OnAre3DAPIsBlocked(int render_frame_id,
                          const GURL& top_origin_url,
                          ThreeDAPIType requester,
                          bool* blocked);

  void OnRenderProcessGone();

#if BUILDFLAG(ENABLE_PLUGINS)
  void OnGetPluginInfo(int render_frame_id,
                       const GURL& url,
                       const url::Origin& main_frame_origin,
                       const std::string& mime_type,
                       bool* found,
                       WebPluginInfo* info,
                       std::string* actual_mime_type);
  void OnOpenChannelToPepperPlugin(
      const base::FilePath& path,
      const base::Optional<url::Origin>& origin_lock,
      IPC::Message* reply_msg);
  void OnDidCreateOutOfProcessPepperInstance(
      int plugin_child_id,
      int32_t pp_instance,
      PepperRendererInstanceData instance_data,
      bool is_external);
  void OnDidDeleteOutOfProcessPepperInstance(int plugin_child_id,
                                             int32_t pp_instance,
                                             bool is_external);
  void OnOpenChannelToPpapiBroker(int routing_id, const base::FilePath& path);
  void OnPluginInstanceThrottleStateChange(int plugin_child_id,
                                           int32_t pp_instance,
                                           bool is_throttled);
#endif  // ENABLE_PLUGINS

#if BUILDFLAG(ENABLE_PLUGINS)
  PluginServiceImpl* plugin_service_;
  base::FilePath profile_data_directory_;

  // Initialized to 0, accessed on FILE thread only.
  base::TimeTicks last_plugin_refresh_time_;
#endif  // ENABLE_PLUGINS

  // The ResourceContext which is to be used on the IO thread.
  ResourceContext* resource_context_;

  // Needed for issuing routing ids and surface ids.
  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  // Whether this process is used for incognito contents.
  bool incognito_;

  const int render_process_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_MESSAGE_FILTER_H_
