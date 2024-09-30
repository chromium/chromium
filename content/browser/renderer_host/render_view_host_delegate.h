// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_

#include <stdint.h>

#include <optional>

#include "base/functional/callback.h"
#include "base/process/kill.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/load_states.h"
#include "services/network/public/mojom/attribution.mojom-forward.h"
#include "third_party/blink/public/common/page/color_provider_color_maps.h"
#include "third_party/skia/include/core/SkColor.h"

namespace blink {
namespace web_pref {
struct WebPreferences;
}
struct RendererPreferences;
}  // namespace blink

namespace content {

class RenderViewHost;
class RenderViewHostDelegateView;

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
//
// Layering note: Generally, WebContentsImpl should be the only implementation
// of this interface. In particular, WebContents::FromRenderViewHost() assumes
// this. This delegate interface is useful for renderer_host/ to make requests
// to WebContentsImpl, as renderer_host/ is not permitted to know the
// WebContents type (see //renderer_host/DEPS).
class RenderViewHostDelegate {
 public:
  // Returns the current delegate associated with a feature. May return NULL if
  // there is no corresponding delegate.
  virtual RenderViewHostDelegateView* GetDelegateView();

  // The `blink::WebView` has been constructed.
  virtual void RenderViewReady(RenderViewHost* render_view_host) {}

  // The process containing the `blink::WebView` exited somehow (either cleanly,
  // crash, or user kill).
  virtual void RenderViewTerminated(RenderViewHost* render_view_host,
                                    base::TerminationStatus status,
                                    int error_code) {}

  // The `blink::WebView` is going to be deleted. This is called when each
  // `blink::WebView` is going to be destroyed
  virtual void RenderViewDeleted(RenderViewHost* render_view_host) {}

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

  // Returns the light, dark and forced color maps for the ColorProvider
  // associated with this RenderViewHost.
  virtual blink::ColorProviderColorMaps GetColorProviderColorMaps() const = 0;

  // Returns true if the render view is rendering a guest.
  virtual bool IsGuest();

  // Called on `blink::WebView` creation to get the initial base background
  // color for this `blink::WebView`. Nullopt means a color is not set, and the
  // blink default color should be used.
  virtual std::optional<SkColor> GetBaseBackgroundColor();

  virtual const base::Location& GetCreatorLocation() = 0;

  // Returns whether attribution reporting is supported
  // for the WebContents associated with this RenderViewHost.
  // This method takes into account global support as well as
  // WebContents specific support.
  virtual network::mojom::AttributionSupport GetAttributionSupport() = 0;

 protected:
  virtual ~RenderViewHostDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_DELEGATE_H_
