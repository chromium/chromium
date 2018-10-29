// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_

#include "base/callback_forward.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_ui_data.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "ui/base/page_transition_types.h"

namespace net {
class URLRequest;
}

namespace content {
class ResourceContext;
class WebContents;

// Each URLRequest allocated by the ResourceDispatcherHost has a
// ResourceRequestInfo instance associated with it.
class ResourceRequestInfo {
 public:
  // Returns the ResourceRequestInfo associated with the given URLRequest.
  CONTENT_EXPORT static ResourceRequestInfo* ForRequest(
      net::URLRequest* request);
  CONTENT_EXPORT static const ResourceRequestInfo* ForRequest(
      const net::URLRequest* request);

  // Allocates a new, dummy ResourceRequestInfo and associates it with the
  // given URLRequest.
  //
  // The RenderView routing ID must correspond to the RenderView of the
  // RenderFrame, both of which share the same RenderProcess. This may be a
  // different RenderView than the WebContents' main RenderView. If the
  // download is not associated with a frame, the IDs can be all -1.
  //
  // NOTE: Add more parameters if you need to initialize other fields.
  CONTENT_EXPORT static void AllocateForTesting(
      net::URLRequest* request,
      ResourceType resource_type,
      ResourceContext* context,
      int render_process_id,
      int render_view_id,
      int render_frame_id,
      bool is_main_frame,
      bool allow_download,
      bool is_async,
      PreviewsState previews_state,
      std::unique_ptr<NavigationUIData> navigation_ui_data);

  // Returns the associated RenderFrame for a given process. Returns false, if
  // there is no associated RenderFrame. This method does not rely on the
  // request being allocated by the ResourceDispatcherHost, but works for all
  // URLRequests that are associated with a RenderFrame.
  CONTENT_EXPORT static bool GetRenderFrameForRequest(
      const net::URLRequest* request,
      int* render_process_id,
      int* render_frame_id);

  // Returns true if the request originated from a Service Worker.
  CONTENT_EXPORT static bool OriginatedFromServiceWorker(
      const net::URLRequest* request);

  // A callback that returns a pointer to a WebContents. The callback can
  // always be used, but it may return nullptr: if the info used to
  // instantiate the callback can no longer be used to return a WebContents,
  // nullptr will be returned instead.
  // The callback should only run on the UI thread and it should always be
  // non-null.
  using WebContentsGetter = base::Callback<WebContents*(void)>;

  // A callback that returns a frame tree node id . The callback can always
  // be used, but it may return -1 if no id is found. The callback should only
  // run on the UI thread.
  using FrameTreeNodeIdGetter = base::Callback<int(void)>;

  // Returns a callback that returns a pointer to the WebContents this request
  // is associated with, or nullptr if it no longer exists or the request is
  // not associated with a WebContents. The callback should only run on the UI
  // thread.
  // Note: Not all resource requests will be owned by a WebContents. For
  // example, requests made by a ServiceWorker.
  virtual WebContentsGetter GetWebContentsGetterForRequest() const = 0;

  // Returns a callback that returns an int with the frame tree node id
  //   associated with this request, or -1 if it no longer exists. This
  //   callback should only be run on the UI thread.
  // Note: Not all resource requests will be associated with a frame. For
  // example, requests made by a ServiceWorker.
  virtual FrameTreeNodeIdGetter GetFrameTreeNodeIdGetterForRequest() const = 0;

  // Returns the associated ResourceContext.
  virtual ResourceContext* GetContext() const = 0;

  // The child process unique ID of the requestor.
  // To get a WebContents, use GetWebContentsGetterForRequest instead.
  virtual int GetChildID() const = 0;

  // The IPC route identifier for this request (this identifies the RenderView
  // or like-thing in the renderer that the request gets routed to).
  // To get a WebContents, use GetWebContentsGetterForRequest instead.
  // Don't use this method for new code, as RenderViews are going away.
  virtual int GetRouteID() const = 0;

  // The globally unique identifier for this request.
  virtual GlobalRequestID GetGlobalRequestID() const = 0;

  // The child process unique ID of the originating process, if the request is
  // was proxied through a renderer process on behalf of a pepper plugin
  // process; -1 otherwise.
  virtual int GetPluginChildID() const = 0;

  // Returns the FrameTreeNode ID for this frame. This ID is browser-global and
  // uniquely identifies a frame that hosts content.
  // Note: Returns -1 for all requests except PlzNavigate requests.
  virtual int GetFrameTreeNodeId() const = 0;

  // The IPC route identifier of the RenderFrame.
  // To get a WebContents, use GetWebContentsGetterForRequest instead.
  // TODO(jam): once all navigation and resource requests are sent between
  // frames and RenderView/RenderViewHost aren't involved we can remove this and
  // just use GetRouteID above.
  virtual int GetRenderFrameID() const = 0;

  // True if GetRenderFrameID() represents a main frame in the RenderView.
  virtual bool IsMainFrame() const = 0;

  // Returns the associated resource type.
  virtual ResourceType GetResourceType() const = 0;

  // Returns the process type that initiated this request.
  virtual int GetProcessType() const = 0;

  // Returns the associated referrer policy.
  virtual network::mojom::ReferrerPolicy GetReferrerPolicy() const = 0;

  // Returns whether the frame that initiated this request is used for
  // prerendering.
  virtual bool IsPrerendering() const = 0;

  // Returns the associated page transition type.
  virtual ui::PageTransition GetPageTransition() const = 0;

  // True if the request was initiated by a user action (like a tap to follow
  // a link).
  //
  // Note that a false value does not mean the request was not initiated by a
  // user gesture. Also note that the fact that a user gesture was active
  // while the request was created does not imply that the user consciously
  // wanted this request to happen nor is aware of it.
  //
  // DO NOT BASE SECURITY DECISIONS ON THIS FLAG!
  virtual bool HasUserGesture() const = 0;

  // Returns false if there is NOT an associated render frame.
  virtual bool GetAssociatedRenderFrame(int* render_process_id,
                                        int* render_frame_id) const = 0;

  // Returns true if this is associated with an asynchronous request.
  virtual bool IsAsync() const = 0;

  // Whether this is a download.
  virtual bool IsDownload() const = 0;

  // Returns the current state of Previews.
  virtual PreviewsState GetPreviewsState() const = 0;

  // PlzNavigate
  // Only used for navigations. Returns opaque data set by the embedder on the
  // UI thread at the beginning of navigation.
  virtual NavigationUIData* GetNavigationUIData() const = 0;

  // Used to annotate requests blocked using net::ERR_BLOCKED_BY_CLIENT and
  // net::ERR_BLOCKED_BY_RESPONSE errors, with a ResourceRequestBlockedReason.
  virtual void SetResourceRequestBlockedReason(
      blink::ResourceRequestBlockedReason) = 0;

  // Returns the ResourceRequestBlockedReason for this request, else
  // base::nullopt.
  virtual base::Optional<blink::ResourceRequestBlockedReason>
  GetResourceRequestBlockedReason() const = 0;

  // When the client of a request decides to cancel it, it may optionally
  // provide an application-defined description of the canncellation reason.
  // This method returns the custom reason. If no such reason has been provided,
  // it returns an empty string.
  virtual base::StringPiece GetCustomCancelReason() const = 0;

 protected:
  virtual ~ResourceRequestInfo() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_REQUEST_INFO_H_
