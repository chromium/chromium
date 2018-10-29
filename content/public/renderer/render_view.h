// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_

#include <stddef.h>

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/browser_controls_state.h"
#include "ipc/ipc_sender.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_widget_types.h"

namespace blink {
class WebElement;
class WebFrameWidget;
class WebView;
struct WebRect;
}  // namespace blink

namespace gfx {
class Size;
}

namespace content {

class RenderFrame;
class RenderViewVisitor;
struct WebPreferences;

// DEPRECATED: RenderView is being removed as part of the SiteIsolation project.
// New code should be added to RenderFrame instead.
//
// For context, please see https://crbug.com/467770 and
// https://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderView : public IPC::Sender {
 public:
  // Returns the RenderView containing the given WebView.
  static RenderView* FromWebView(blink::WebView* webview);

  // Returns the RenderView for the given routing ID.
  static RenderView* FromRoutingID(int routing_id);

  // Returns the number of live RenderView instances in this process.
  static size_t GetRenderViewCount();

  // Visit all RenderViews with a live WebView (i.e., RenderViews that have
  // been closed but not yet destroyed are excluded).
  static void ForEach(RenderViewVisitor* visitor);

  // Applies WebKit related preferences to this view.
  static void ApplyWebPreferences(const WebPreferences& preferences,
                                  blink::WebView* web_view);

  // Returns the main RenderFrame.
  virtual RenderFrame* GetMainRenderFrame() = 0;

  // Get the routing ID of the view.
  virtual int GetRoutingID() const = 0;

  // Returns the size of the view.
  virtual gfx::Size GetSize() const = 0;

  // Returns the device scale factor of the display the render view is in.
  virtual float GetDeviceScaleFactor() const = 0;

  // Returns the page's zoom level for the render view.
  virtual float GetZoomLevel() const = 0;

  // Gets WebKit related preferences associated with this view.
  virtual const WebPreferences& GetWebkitPreferences() = 0;

  // Overrides the WebKit related preferences associated with this view. Note
  // that the browser process may update the preferences at any time.
  virtual void SetWebkitPreferences(const WebPreferences& preferences) = 0;

  // Returns the associated WebView. May return NULL when the view is closing.
  virtual blink::WebView* GetWebView() = 0;

  // Returns the associated WebFrameWidget.
  virtual blink::WebFrameWidget* GetWebFrameWidget() = 0;

  // Whether content state (such as form state, scroll position and page
  // contents) should be sent to the browser immediately. This is normally
  // false, but set to true by some tests.
  virtual bool GetContentStateImmediately() const = 0;

  // Inject edit commands to be used for the next keyboard event.
  // TODO(alexmos): Currently, these are used only by BlinkTestRunner.  They
  // should be removed from RenderView and instead be plumbed through the
  // target frame and WebFrameTestProxy.
  virtual void SetEditCommandForNextKeyEvent(const std::string& name,
                                             const std::string& value) = 0;
  virtual void ClearEditCommands() = 0;

  // Returns |renderer_preferences_.accept_languages| value.
  virtual const std::string& GetAcceptLanguages() const = 0;

  virtual void UpdateBrowserControlsState(BrowserControlsState constraints,
                                          BrowserControlsState current,
                                          bool animate) = 0;

  // Converts the |rect| from Viewport coordinates to Window coordinates.
  // See blink::WebWidgetClient::convertViewportToWindow for more details.
  virtual void ConvertViewportToWindowViaWidget(blink::WebRect* rect) = 0;

  // Returns the bounds of |element| in Window coordinates. The bounds have been
  // adjusted to include any transformations, including page scale.
  // This function will update the layout if required.
  virtual gfx::RectF ElementBoundsInWindow(const blink::WebElement& element)
      = 0;

 protected:
  ~RenderView() override {}

 private:
  // This interface should only be implemented inside content.
  friend class RenderViewImpl;
  RenderView() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_
