// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PageNavigator defines an interface that can be used to express the user's
// intention to navigate to a particular URL.  The implementing class should
// perform the navigation.

#ifndef CONTENT_PUBLIC_BROWSER_PAGE_NAVIGATOR_H_
#define CONTENT_PUBLIC_BROWSER_PAGE_NAVIGATOR_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/referrer.h"
#include "ipc/ipc_message.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/triggering_event_info.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace content {

class NavigationHandle;
class WebContents;

struct CONTENT_EXPORT OpenURLParams {
  OpenURLParams(const GURL& url,
                const Referrer& referrer,
                WindowOpenDisposition disposition,
                ui::PageTransition transition,
                bool is_renderer_initiated);
  OpenURLParams(const GURL& url,
                const Referrer& referrer,
                WindowOpenDisposition disposition,
                ui::PageTransition transition,
                bool is_renderer_initiated,
                bool started_from_context_menu);
  OpenURLParams(const GURL& url,
                const Referrer& referrer,
                FrameTreeNodeId frame_tree_node_id,
                WindowOpenDisposition disposition,
                ui::PageTransition transition,
                bool is_renderer_initiated);
  OpenURLParams(const OpenURLParams& other);
  ~OpenURLParams();

  // Creates OpenURLParams that 1) preserve all applicable |handle| properties
  // (URL, referrer, initiator, etc.) with OpenURLParams equivalents and 2) fill
  // in reasonable defaults for other properties (like WindowOpenDisposition).
  static OpenURLParams FromNavigationHandle(NavigationHandle* handle);

#if DCHECK_IS_ON()
  // Returns true if the contents of this struct are considered valid and
  // satisfy dependencies between fields (e.g. about:blank URLs require
  // |initiator_origin| and |source_site_instance| to be set).
  bool Valid() const;
#endif

  // The URL/referrer to be opened.
  GURL url;
  Referrer referrer;

  // The frame token of the initiator of the navigation. This is best effort: it
  // is only defined for some renderer-initiated navigations (e.g., not drag and
  // drop), and the frame with the corresponding token may have been deleted
  // before the navigation begins. This parameter is defined if and only if
  // |initiator_process_id| below is.
  std::optional<blink::LocalFrameToken> initiator_frame_token;

  // ID of the renderer process of the RenderFrameHost that initiated the
  // navigation. This is defined if and only if |initiator_frame_token| above
  // is, and it is only valid in conjunction with it.
  int initiator_process_id = ChildProcessHost::kInvalidUniqueID;

  // The origin of the initiator of the navigation.
  std::optional<url::Origin> initiator_origin;

  // The base url of the initiator of the navigation. This will be non-null only
  // if the navigation is about:blank or about:srcdoc.
  std::optional<GURL> initiator_base_url;

  // SiteInstance of the frame that initiated the navigation or null if we
  // don't know it.
  scoped_refptr<content::SiteInstance> source_site_instance;

  // Any redirect URLs that occurred for this navigation before |url|.
  std::vector<GURL> redirect_chain;

  // The post data when the navigation uses POST.
  scoped_refptr<network::ResourceRequestBody> post_data;

  // Extra headers to add to the request for this page.  Headers are
  // represented as "<name>: <value>" and separated by \r\n.  The entire string
  // is terminated by \r\n.  May be empty if no extra headers are needed.
  std::string extra_headers;

  // The browser-global FrameTreeNode ID for the frame to navigate, or the
  // default-constructed invalid value to indicate the main frame.
  FrameTreeNodeId frame_tree_node_id;

  // Routing id of the source RenderFrameHost.
  int source_render_frame_id = MSG_ROUTING_NONE;

  // Process id of the source RenderFrameHost.
  int source_render_process_id = ChildProcessHost::kInvalidUniqueID;

  // The disposition requested by the navigation source.
  WindowOpenDisposition disposition;

  // The transition type of navigation.
  ui::PageTransition transition;

  // Whether this navigation is initiated by the renderer process.
  bool is_renderer_initiated;

  // Indicates whether this navigation should replace the current
  // navigation entry.
  bool should_replace_current_entry = false;

  // Indicates whether this navigation was triggered while processing a user
  // gesture if the navigation was initiated by the renderer.
  bool user_gesture;

  // Whether the call to OpenURL was triggered by an Event, and what the
  // isTrusted flag of the event was.
  blink::mojom::TriggeringEventInfo triggering_event_info =
      blink::mojom::TriggeringEventInfo::kUnknown;

  // Indicates whether this navigation was started via context menu.
  bool started_from_context_menu = false;

  // Optional URLLoaderFactory to facilitate navigation to a blob URL.
  scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;

  // Indicates that the navigation should happen in an app window if
  // possible, i.e. if an app for the URL is installed.
  bool open_app_window_if_possible = false;

  // If this navigation was initiated from a link that specified the
  // hrefTranslate attribute, this contains the attribute's value (a BCP47
  // language code). Empty otherwise.
  std::string href_translate;

  // Indicates if this navigation is a reload.
  ReloadType reload_type = ReloadType::NONE;

  // Optional impression associated with this navigation. Only set on
  // navigations that originate from links with impression attributes. Used for
  // conversion measurement.
  std::optional<blink::Impression> impression;

  // Indicates that this navigation is for PDF content in a renderer.
  bool is_pdf = false;

  // True if the initiator explicitly asked for opener relationships to be
  // preserved, via rel="opener".
  bool has_rel_opener = false;
};

class PageNavigator {
 public:
  virtual ~PageNavigator() = default;

  // Opens a URL using parameters from `params`.
  // Returns:
  //    * A pointer to the WebContents object where the URL is opened.
  //    * nullptr if the URL could not be opened immediately.
  //
  // If a `navigation_handle_callback` function is provided, it should be called
  // with the pending navigation (if any) when the navigation handle become
  // available. This allows callers to observe or attach their specific data.
  // This function may not be called if the navigation fails for any reason.
  virtual WebContents* OpenURL(const OpenURLParams& params,
                               base::OnceCallback<void(NavigationHandle&)>
                                   navigation_handle_callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAGE_NAVIGATOR_H_
