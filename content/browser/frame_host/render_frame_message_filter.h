// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_MESSAGE_FILTER_H_

#include <stdint.h>

#include <set>

#include "base/optional.h"
#include "content/common/frame_replication_state.h"
#include "content/common/render_frame_message_filter.mojom.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/three_d_api_types.h"
#include "net/cookies/canonical_cookie.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/web/web_tree_scope_type.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/common/pepper_renderer_instance_data.h"
#endif

struct FrameHostMsg_CreateChildFrame_Params;
struct FrameHostMsg_DownloadUrl_Params;
class GURL;

namespace mojo {
class MessagePipeHandle;
}

namespace net {
class URLRequestContextGetter;
}

namespace url {
class Origin;
}

namespace content {
class BrowserContext;
class PluginServiceImpl;
struct Referrer;
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
class CONTENT_EXPORT RenderFrameMessageFilter
    : public BrowserMessageFilter,
      public BrowserAssociatedInterface<mojom::RenderFrameMessageFilter>,
      public mojom::RenderFrameMessageFilter {
 public:
  RenderFrameMessageFilter(int render_process_id,
                           PluginServiceImpl* plugin_service,
                           BrowserContext* browser_context,
                           StoragePartition* storage_partition,
                           RenderWidgetHelper* render_widget_helper);

  // BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

  network::mojom::CookieManagerPtr* GetCookieManager();

 protected:
  friend class TestSaveImageFromDataURL;

  // This method will be overridden by TestSaveImageFromDataURL class for test.
  virtual void DownloadUrl(
      int render_view_id,
      int render_frame_id,
      const GURL& url,
      const Referrer& referrer,
      const url::Origin& initiator,
      const base::string16& suggested_name,
      const bool use_prompt,
      const bool follow_cross_origin_redirects,
      blink::mojom::BlobURLTokenPtrInfo blob_url_token) const;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<RenderFrameMessageFilter>;

  class OpenChannelToPpapiPluginCallback;
  class OpenChannelToPpapiBrokerCallback;

  ~RenderFrameMessageFilter() override;

  void InitializeCookieManager(
      network::mojom::CookieManagerRequest cookie_manager_request);

  // |new_render_frame_id| and |devtools_frame_token| are out parameters.
  // Browser process defines them for the renderer process.
  void OnCreateChildFrame(const FrameHostMsg_CreateChildFrame_Params& params,
                          int* new_render_frame_id,
                          mojo::MessagePipeHandle* new_interface_provider,
                          base::UnguessableToken* devtools_frame_token);
  void OnCookiesEnabled(int render_frame_id,
                        const GURL& url,
                        const GURL& site_for_cookies,
                        bool* cookies_enabled);

  // Check the policy for getting cookies. Gets the cookies if allowed.
  void CheckPolicyForCookies(int render_frame_id,
                             const GURL& url,
                             const GURL& site_for_cookies,
                             GetCookiesCallback callback,
                             const net::CookieList& cookie_list);

  void OnDownloadUrl(const FrameHostMsg_DownloadUrl_Params& params);

  void OnSaveImageFromDataURL(int render_view_id,
                              int render_frame_id,
                              const std::string& url_str);

  void OnAre3DAPIsBlocked(int render_frame_id,
                          const GURL& top_origin_url,
                          ThreeDAPIType requester,
                          bool* blocked);

  void OnRenderProcessGone();

  // mojom::RenderFrameMessageFilter:
  void SetCookie(int32_t render_frame_id,
                 const GURL& url,
                 const GURL& site_for_cookies,
                 const std::string& cookie_line,
                 SetCookieCallback callback) override;
  void GetCookies(int render_frame_id,
                  const GURL& url,
                  const GURL& site_for_cookies,
                  GetCookiesCallback callback) override;

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
  void OnOpenChannelToPpapiBroker(int routing_id,
                                  const base::FilePath& path);
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

  // Contextual information to be used for requests created here.
  scoped_refptr<net::URLRequestContextGetter> request_context_;

  // The ResourceContext which is to be used on the IO thread.
  ResourceContext* resource_context_;

  network::mojom::CookieManagerPtr cookie_manager_;

  // Needed for issuing routing ids and surface ids.
  scoped_refptr<RenderWidgetHelper> render_widget_helper_;

  // Whether this process is used for incognito contents.
  bool incognito_;

  const int render_process_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RENDER_FRAME_MESSAGE_FILTER_H_
