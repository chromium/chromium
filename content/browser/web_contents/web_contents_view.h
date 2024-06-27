// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_H_

#include <string>

#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

class BackForwardTransitionAnimationManager;
class RenderViewHost;
class RenderViewHostDelegateView;
class RenderWidgetHost;
class RenderWidgetHostViewBase;
class WebContentsImpl;
class WebContentsViewDelegate;
struct DropData;

// The `WebContentsView` is an interface that is implemented by the platform-
// dependent web contents views. The `WebContents` uses this interface to talk
// to them.
class WebContentsView {
 public:
  virtual ~WebContentsView() = default;

  // Returns the native widget that contains the contents of the tab.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Returns the native widget with the main content of the tab (i.e. the main
  // render view host, though there may be many popups in the tab as children of
  // the container).
  virtual gfx::NativeView GetContentNativeView() const = 0;

  // Returns the outermost native view. This will be used as the parent for
  // dialog boxes.
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const = 0;

  // Computes the rectangle for the native widget that contains the contents of
  // the tab in the screen coordinate system.
  virtual gfx::Rect GetContainerBounds() const = 0;

  // Sets focus to the native widget for this tab.
  virtual void Focus() = 0;

  // Sets focus to the appropriate element when the WebContents is shown the
  // first time.
  virtual void SetInitialFocus() = 0;

  // Stores the currently focused view.
  virtual void StoreFocus() = 0;

  // Restores focus to the last focus view. If StoreFocus has not yet been
  // invoked, SetInitialFocus is invoked.
  virtual void RestoreFocus() = 0;

  // Focuses the first (last if |reverse| is true) element in the page.
  // Invoked when this tab is getting the focus through tab traversal (|reverse|
  // is true when using Shift-Tab).
  virtual void FocusThroughTabTraversal(bool reverse) = 0;

  // Returns the current drop data, if any.
  virtual DropData* GetDropData() const = 0;

  // Get the bounds of the View in the global screen position.
  virtual gfx::Rect GetViewBounds() const = 0;

  virtual void CreateView(gfx::NativeView context) = 0;

  // Sets up the View that holds the rendered web page, receives messages for
  // it and contains page plugins. The host view should be sized to the current
  // size of the WebContents.
  virtual RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) = 0;

  // Creates a new View that holds a non-top-level widget and receives messages
  // for it.
  virtual RenderWidgetHostViewBase* CreateViewForChildWidget(
      RenderWidgetHost* render_widget_host) = 0;

  // Sets the page title for the native widgets corresponding to the view. This
  // is not strictly necessary and isn't expected to be displayed anywhere, but
  // can aid certain debugging tools such as Spy++ on Windows where you are
  // trying to find a specific window.
  virtual void SetPageTitle(const std::u16string& title) = 0;

  // Invoked when the WebContents is notified that the `blink::WebView` is
  // ready.
  virtual void RenderViewReady() = 0;

  // Invoked when the WebContents is notified that the RenderViewHost has been
  // changed.
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) = 0;

  // Invoked to enable/disable overscroll gesture navigation.
  virtual void SetOverscrollControllerEnabled(bool enabled) = 0;

  // Called when the capturer-count of the WebContents changes.
  virtual void OnCapturerCountChanged() = 0;

#if BUILDFLAG(IS_MAC)
  // If we close the tab while a UI control is in an event-tracking loop, the
  // the control may message freed objects and crash. WebContents::Close will
  // call this. If it returns true, then WebContents::Close will early-out, and
  // it will be the responsibility of |this| to call CloseTab when the nested
  // loop has ended.
  virtual bool CloseTabAfterEventTrackingIfNeeded() = 0;
#endif

  virtual void FullscreenStateChanged(bool is_fullscreen) = 0;

  // Intended for desktop PWAs with manifest entry of window-controls-overlay,
  // this informs the view of which area at the top of the view is available for
  // web contents.
  virtual void UpdateWindowControlsOverlay(const gfx::Rect& bounding_rect) = 0;

  // Returns an animation manager that displays a preview of the history page
  // during a session history navigation gesture. Only non-null if
  // `features::kBackForwardTransitions` is enabled for the supported platform.
  virtual BackForwardTransitionAnimationManager*
  GetBackForwardTransitionAnimationManager() = 0;
};

// Factory function to create `WebContentsView`s. Implemented in the platform
// files.
std::unique_ptr<WebContentsView> CreateWebContentsView(
    WebContentsImpl* web_contents,
    std::unique_ptr<WebContentsViewDelegate> delegate,
    raw_ptr<RenderViewHostDelegateView>* render_view_host_delegate_view);

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_H_
