// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_

#include <stdint.h>

#include <string>

#include "base/callback.h"
#include "base/process/kill.h"
#include "base/strings/string16.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/common/content_export.h"
#include "content/common/render_message_filter.mojom.h"
#include "content/common/widget.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_states.h"

class GURL;

namespace IPC {
class Message;
}

namespace blink {
namespace mojom {
class RendererPreferences;
}
}  // namespace blink

namespace gfx {
class Rect;
class Size;
}

namespace content {

class BrowserContext;
class FrameTree;
class RenderFrameHostImpl;
class RenderViewHost;
class RenderViewHostImpl;
class RenderViewHostDelegateView;
class SessionStorageNamespace;
class SiteInstance;
class WebContents;

//
// RenderViewHostDelegate
//
//  An interface implemented by an object interested in knowing about the state
//  of the RenderViewHost.
//
//  This interface currently encompasses every type of message that was
//  previously being sent by WebContents itself. Some of these notifications
//  may not be relevant to all users of RenderViewHost and we should consider
//  exposing a more generic Send function on RenderViewHost and a response
//  listener here to serve that need.
class CONTENT_EXPORT RenderViewHostDelegate {
 public:
  // Returns the current delegate associated with a feature. May return NULL if
  // there is no corresponding delegate.
  virtual RenderViewHostDelegateView* GetDelegateView();

  // This is used to give the delegate a chance to filter IPC messages.
  virtual bool OnMessageReceived(RenderViewHostImpl* render_view_host,
                                 const IPC::Message& message);

  // Return this object cast to a WebContents, if it is one. If the object is
  // not a WebContents, returns NULL. DEPRECATED: Be sure to include brettw or
  // jam as reviewers before you use this method. http://crbug.com/82582
  virtual WebContents* GetAsWebContents();

  // The RenderView is being constructed (message sent to the renderer process
  // to construct a RenderView).  Now is a good time to send other setup events
  // to the RenderView.  This precedes any other commands to the RenderView.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) {}

  // The RenderView has been constructed.
  virtual void RenderViewReady(RenderViewHost* render_view_host) {}

  // The process containing the RenderView exited somehow (either cleanly,
  // crash, or user kill).
  virtual void RenderViewTerminated(RenderViewHost* render_view_host,
                                    base::TerminationStatus status,
                                    int error_code) {}

  // The RenderView is going to be deleted. This is called when each
  // RenderView is going to be destroyed
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

  // The destination URL has changed should be updated.
  virtual void UpdateTargetURL(RenderViewHost* render_view_host,
                               const GURL& url) {}

  // The page is trying to close the RenderView's representation in the client.
  virtual void Close(RenderViewHost* render_view_host) {}

  // The page is trying to move the RenderView's representation in the client.
  virtual void RequestSetBounds(const gfx::Rect& new_bounds) {}

  // The RenderView's main frame document element is ready. This happens when
  // the document has finished parsing.
  virtual void DocumentAvailableInMainFrame(RenderViewHost* render_view_host) {}

  // The page wants to close the active view in this tab.
  virtual void RouteCloseEvent(RenderViewHost* rvh) {}

  // Return a dummy RendererPreferences object that will be used by the renderer
  // associated with the owning RenderViewHost.
  virtual blink::mojom::RendererPreferences GetRendererPrefs(
      BrowserContext* browser_context) const = 0;

  // Notification from the renderer host that blocked UI event occurred.
  // This happens when there are tab-modal dialogs. In this case, the
  // notification is needed to let us draw attention to the dialog (i.e.
  // refocus on the modal dialog, flash title etc).
  virtual void OnIgnoredUIEvent() {}

  // The page wants the hosting window to activate itself (it called the
  // JavaScript window.focus() method).
  virtual void Activate() {}

  // The contents' preferred size changed.
  virtual void UpdatePreferredSize(const gfx::Size& pref_size) {}

  // The page is trying to open a new widget (e.g. a select popup). The
  // widget should be created associated with the given |route_id| in the
  // process |render_process_id|, but it should not be shown yet. That should
  // happen in response to ShowCreatedWidget.
  virtual void CreateNewWidget(int32_t render_process_id,
                               int32_t widget_route_id,
                               mojo::PendingRemote<mojom::Widget> widget,
                               RenderViewHostImpl* render_view_host) {}

  // Creates a full screen RenderWidget. Similar to above.
  virtual void CreateNewFullscreenWidget(
      int32_t render_process_id,
      int32_t widget_route_id,
      mojo::PendingRemote<mojom::Widget> widget,
      RenderViewHostImpl* render_view_host) {}

  // Show the newly created widget with the specified bounds.
  // The widget is identified by the route_id passed to CreateNewWidget.
  virtual void ShowCreatedWidget(int process_id,
                                 int widget_route_id,
                                 const gfx::Rect& initial_rect) {}

  // Show the newly created full screen widget. Similar to above.
  virtual void ShowCreatedFullscreenWidget(int process_id,
                                           int widget_route_id) {}

  // Returns the SessionStorageNamespace the render view should use. Might
  // create the SessionStorageNamespace on the fly.
  virtual SessionStorageNamespace* GetSessionStorageNamespace(
      SiteInstance* instance);

  // Returns a copy of the map of all session storage namespaces related
  // to this view.
  virtual SessionStorageNamespaceMap GetSessionStorageNamespaceMap();

  // Returns true if the RenderViewHost will never be visible.
  virtual bool IsNeverVisible();

  // Returns the FrameTree the render view should use. Guaranteed to be constant
  // for the lifetime of the render view.
  //
  // TODO(ajwong): Remove once the main frame RenderFrameHost is no longer
  // created by the RenderViewHost.
  virtual FrameTree* GetFrameTree();

  // Whether the user agent is overridden using the Chrome for Android "Request
  // Desktop Site" feature.
  virtual bool IsOverridingUserAgent();

  virtual bool IsJavaScriptDialogShowing() const;

  // If a timer for an unresponsive renderer fires, whether it should be
  // ignored.
  virtual bool ShouldIgnoreUnresponsiveRenderer();

  // Whether download UI should be hidden.
  virtual bool HideDownloadUI() const;

  // Whether the WebContents as a persistent video.
  virtual bool HasPersistentVideo() const;

  // Whether spatial navigation is permitted.
  virtual bool IsSpatialNavigationDisabled() const;

  // Returns the RenderFrameHost for a pending or speculative main frame
  // navigation for the page.  Returns nullptr if there is no such navigation.
  virtual RenderFrameHostImpl* GetPendingMainFrame();

  // The RenderView finished the first visually non-empty paint.
  virtual void DidFirstVisuallyNonEmptyPaint(RenderViewHostImpl* source) {}

  // Returns true if the render view is rendering a portal.
  virtual bool IsPortal() const;

  // Called when the theme color for the underlying document as specified
  // by theme-color meta tag has changed.
  virtual void OnThemeColorChanged(RenderViewHostImpl* source) {}

 protected:
  virtual ~RenderViewHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
