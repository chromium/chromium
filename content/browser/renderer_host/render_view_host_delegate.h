// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/process/kill.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_states.h"

namespace IPC {
class Message;
}

namespace blink {
namespace web_pref {
struct WebPreferences;
}
struct RendererPreferences;
}  // namespace blink

namespace gfx {
class Size;
}

namespace content {

class RenderViewHost;
class RenderViewHostImpl;
class RenderViewHostDelegateView;
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

  // The page is trying to close the RenderView's representation in the client.
  virtual void Close(RenderViewHost* render_view_host) {}

  // Return a dummy RendererPreferences object that will be used by the renderer
  // associated with the owning RenderViewHost.
  virtual const blink::RendererPreferences& GetRendererPrefs() const = 0;

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

  // Returns a copy of the map of all session storage namespaces related
  // to this view.
  virtual SessionStorageNamespaceMap GetSessionStorageNamespaceMap();

  // Returns true if RenderWidgets under this RenderViewHost will never be
  // user-visible and thus never need to generate pixels for display.
  virtual bool IsNeverComposited();

  // Returns a copy of the current WebPreferences associated with this
  // RenderViewHost's WebContents. If it does not exist, this will create one
  // and send the newly computed value to all renderers.
  // Note that this will not trigger a recomputation of WebPreferences if it
  // already exists - this will return the last computed/set value of
  // WebPreferences. If we want to guarantee that the value reflects the current
  // state of the WebContents, NotifyPreferencesChanged() should be called
  // before calling this.
  virtual const blink::web_pref::WebPreferences&
  GetOrCreateWebPreferences() = 0;

  // Sets the WebPreferences for the WebContents associated with this
  // RenderViewHost to |prefs| and send the new value to all renderers in the
  // WebContents.
  virtual void SetWebPreferences(const blink::web_pref::WebPreferences& prefs) {
  }

  // Triggers a total recomputation of WebPreferences by resetting the current
  // cached WebPreferences to null and triggering the recomputation path for
  // both the "slow" attributes (hardware configurations/things that require
  // slow platform/device polling) which normally won't get recomputed after
  // the first time we set it and "fast" attributes (which always gets
  // recomputed).
  virtual void RecomputeWebPreferencesSlow() {}

  virtual bool IsJavaScriptDialogShowing() const;

  // If a timer for an unresponsive renderer fires, whether it should be
  // ignored.
  virtual bool ShouldIgnoreUnresponsiveRenderer();

  // The RenderView finished the first visually non-empty paint.
  virtual void DidFirstVisuallyNonEmptyPaint(RenderViewHostImpl* source) {}

  // Returns true if the render view is rendering a guest.
  virtual bool IsGuest();

  // Returns true if the render view is rendering a portal.
  virtual bool IsPortal();

  // Called when the theme color for the underlying document as specified
  // by theme-color meta tag has changed.
  virtual void OnThemeColorChanged(RenderViewHostImpl* source) {}

  // Called when the CSS background color for the underlying document has
  // changed.
  virtual void OnBackgroundColorChanged(RenderViewHostImpl* source) {}

 protected:
  virtual ~RenderViewHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
