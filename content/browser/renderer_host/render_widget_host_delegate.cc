// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_delegate.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

KeyboardEventProcessingResult RenderWidgetHostDelegate::PreHandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  return KeyboardEventProcessingResult::NOT_HANDLED;
}

bool RenderWidgetHostDelegate::PreHandleMouseEvent(
    const blink::WebMouseEvent& event) {
  return false;
}

bool RenderWidgetHostDelegate::HandleMouseEvent(
    const blink::WebMouseEvent& event) {
  return false;
}

bool RenderWidgetHostDelegate::HandleWheelEvent(
    const blink::WebMouseWheelEvent& event) {
  return false;
}

bool RenderWidgetHostDelegate::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  return false;
}

bool RenderWidgetHostDelegate::ShouldIgnoreInputEvents() {
  return false;
}

bool RenderWidgetHostDelegate::PreHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  return false;
}

double RenderWidgetHostDelegate::GetPendingPageZoomLevel() {
  return 0.0;
}

BrowserAccessibilityManager*
    RenderWidgetHostDelegate::GetRootBrowserAccessibilityManager() {
  return nullptr;
}

BrowserAccessibilityManager*
    RenderWidgetHostDelegate::GetOrCreateRootBrowserAccessibilityManager() {
  return nullptr;
}

// If a delegate does not override this, the RenderWidgetHostView will
// assume it is the sole platform event consumer.
RenderWidgetHostInputEventRouter*
RenderWidgetHostDelegate::GetInputEventRouter() {
  return nullptr;
}

// If a delegate does not override this, the RenderWidgetHostView will
// assume its own RenderWidgetHost should consume keyboard events.
RenderWidgetHostImpl* RenderWidgetHostDelegate::GetFocusedRenderWidgetHost(
    RenderWidgetHostImpl* receiving_widget) {
  return receiving_widget;
}

RenderWidgetHostImpl*
RenderWidgetHostDelegate::GetRenderWidgetHostWithPageFocus() {
  return nullptr;
}

bool RenderWidgetHostDelegate::IsFullscreen() {
  return false;
}

bool RenderWidgetHostDelegate::ShouldShowStaleContentOnEviction() {
  return false;
}

blink::mojom::DisplayMode RenderWidgetHostDelegate::GetDisplayMode() const {
  return blink::mojom::DisplayMode::kBrowser;
}

gfx::Rect RenderWidgetHostDelegate::GetWindowsControlsOverlayRect() const {
  return gfx::Rect();
}

bool RenderWidgetHostDelegate::HasMouseLock(
    RenderWidgetHostImpl* render_widget_host) {
  return false;
}

RenderWidgetHostImpl* RenderWidgetHostDelegate::GetMouseLockWidget() {
  return nullptr;
}

bool RenderWidgetHostDelegate::RequestKeyboardLock(RenderWidgetHostImpl* host,
                                                   bool esc_key_locked) {
  return false;
}

RenderWidgetHostImpl* RenderWidgetHostDelegate::GetKeyboardLockWidget() {
  return nullptr;
}

bool RenderWidgetHostDelegate::OnRenderFrameProxyVisibilityChanged(
    RenderFrameProxyHost* render_frame_proxy_host,
    blink::mojom::FrameVisibility visibility) {
  return false;
}

TextInputManager* RenderWidgetHostDelegate::GetTextInputManager() {
  return nullptr;
}

RenderViewHostDelegateView* RenderWidgetHostDelegate::GetDelegateView() {
  return nullptr;
}

bool RenderWidgetHostDelegate::IsWidgetForPrimaryMainFrame(
    RenderWidgetHostImpl*) {
  return false;
}

ukm::SourceId RenderWidgetHostDelegate::GetCurrentPageUkmSourceId() {
  return ukm::kInvalidSourceId;
}

bool RenderWidgetHostDelegate::IsShowingContextMenuOnPage() const {
  return false;
}

bool RenderWidgetHostDelegate::IsPortal() {
  return false;
}

int RenderWidgetHostDelegate::GetVirtualKeyboardResizeHeight() {
  return 0;
}

}  // namespace content
