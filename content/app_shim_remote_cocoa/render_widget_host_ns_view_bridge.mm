// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/render_widget_host_ns_view_bridge.h"

#include <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#include "content/app_shim_remote_cocoa/render_widget_host_ns_view_host_helper.h"
#include "content/common/mac/attributed_string_type_converters.h"
#import "skia/ext/skia_utils_mac.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#import "ui/base/cocoa/animation_utils.h"
#import "ui/base/cocoa/cursor_utils.h"
#include "ui/base/mojom/attributed_string.mojom.h"
#include "ui/display/screen.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/mac/coordinate_conversion.h"

using blink::WebGestureEvent;

namespace remote_cocoa {

RenderWidgetHostNSViewBridge::RenderWidgetHostNSViewBridge(
    mojom::RenderWidgetHostNSViewHost* host,
    RenderWidgetHostNSViewHostHelper* host_helper,
    uint64_t ns_view_id,
    base::OnceClosure destroy_callback)
    : destroy_callback_(std::move(destroy_callback)) {
  cocoa_view_ = [[RenderWidgetHostViewCocoa alloc] initWithHost:host
                                                 withHostHelper:host_helper];

  background_layer_ = [[CALayer alloc] init];
  display_ca_layer_tree_ =
      std::make_unique<ui::DisplayCALayerTree>(background_layer_);
  cocoa_view_.layer = background_layer_;
  cocoa_view_.wantsLayer = YES;

  view_id_ = std::make_unique<remote_cocoa::ScopedNSViewIdMapping>(ns_view_id,
                                                                   cocoa_view_);
}

RenderWidgetHostNSViewBridge::~RenderWidgetHostNSViewBridge() {
  [cocoa_view_ setHostDisconnected];
  // Do not immediately remove |cocoa_view_| from the NSView hierarchy, because
  // the call to -[NSView removeFromSuperview] may cause us to call into the
  // RWHVMac during tear-down, via WebContentsImpl::UpdateWebContentsVisibility.
  // https://crbug.com/834931
  [cocoa_view_ performSelector:@selector(removeFromSuperview)
                    withObject:nil
                    afterDelay:0];
  popup_window_.reset();
}

void RenderWidgetHostNSViewBridge::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::RenderWidgetHostNSView>
        bridge_receiver) {
  receiver_.Bind(std::move(bridge_receiver),
                 ui::WindowResizeHelperMac::Get()->task_runner());
}

RenderWidgetHostViewCocoa* RenderWidgetHostNSViewBridge::GetNSView() {
  return cocoa_view_;
}

void RenderWidgetHostNSViewBridge::InitAsPopup(
    const gfx::Rect& content_rect,
    uint64_t popup_parent_ns_view_id) {
  popup_window_ = std::make_unique<PopupWindowMac>(content_rect, cocoa_view_);

  [cocoa_view_ setPopupParentNSViewId:popup_parent_ns_view_id];
}

void RenderWidgetHostNSViewBridge::SetParentWebContentsNSView(
    uint64_t parent_ns_view_id) {
  NSView* parent_ns_view = remote_cocoa::GetNSViewFromId(parent_ns_view_id);
  // If the browser passed an invalid handle, then there is no recovery.
  CHECK(parent_ns_view);
  // Set the frame and autoresizing mask of the RenderWidgetHostViewCocoa as is
  // done by WebContentsViewMac.
  cocoa_view_.frame = parent_ns_view.bounds;
  cocoa_view_.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  // Place the new view below all other views, matching the behavior in
  // WebContentsViewMac::CreateViewForWidget.
  // https://crbug.com/1017446
  [parent_ns_view addSubview:cocoa_view_
                  positioned:NSWindowBelow
                  relativeTo:nil];
}

void RenderWidgetHostNSViewBridge::MakeFirstResponder() {
  [cocoa_view_.window makeFirstResponder:cocoa_view_];
}

void RenderWidgetHostNSViewBridge::DisableDisplay() {
  if (display_disabled_)
    return;
  SetBackgroundColor(SK_ColorTRANSPARENT);
  display_ca_layer_tree_.reset();
  display_disabled_ = true;
}

void RenderWidgetHostNSViewBridge::SetBounds(const gfx::Rect& rect) {
  // |rect.size()| is view coordinates, |rect.origin| is screen coordinates,
  // TODO(thakis): fix, http://crbug.com/73362

  // During the initial creation of the RenderWidgetHostView in
  // WebContentsImpl::CreateRenderViewForRenderManager, SetSize is called with
  // an empty size. In the Windows code flow, it is not ignored because
  // subsequent sizing calls from the OS flow through TCVW::WasSized which calls
  // SetSize() again. On Cocoa, we rely on the Cocoa view struture and resizer
  // flags to keep things sized properly. On the other hand, if the size is not
  // empty then this is a valid request for a pop-up.
  if (rect.size().IsEmpty())
    return;

  // Ignore the position of |rect| for non-popup rwhvs. This is because
  // background tabs do not have a window, but the window is required for the
  // coordinate conversions. Popups are always for a visible tab.
  //
  // Note: If |cocoa_view_| has been removed from the view hierarchy, it's still
  // valid for resizing to be requested (e.g., during tab capture, to size the
  // view to screen-capture resolution). In this case, simply treat the view as
  // relative to the screen.
  BOOL isRelativeToScreen =
      IsPopup() || ![cocoa_view_.superview isKindOfClass:[BaseView class]];
  if (isRelativeToScreen) {
    // The position of |rect| is screen coordinate system and we have to
    // consider Cocoa coordinate system is upside-down and also multi-screen.
    NSRect frame = gfx::ScreenRectToNSRect(rect);
    if (IsPopup())
      [popup_window_->window() setFrame:frame display:YES];
    else
      cocoa_view_.frame = frame;
  } else {
    BaseView* superview = static_cast<BaseView*>(cocoa_view_.superview);
    gfx::Rect rect2 = [superview flipNSRectToRect:cocoa_view_.frame];
    rect2.set_width(rect.width());
    rect2.set_height(rect.height());
    cocoa_view_.frame = [superview flipRectToNSRect:rect2];
  }
}

void RenderWidgetHostNSViewBridge::SetCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
  if (display_disabled_)
    return;
  display_ca_layer_tree_->UpdateCALayerTree(ca_layer_params);
}

void RenderWidgetHostNSViewBridge::SetBackgroundColor(SkColor color) {
  if (display_disabled_)
    return;
  ScopedCAActionDisabler disabler;
  background_layer_.backgroundColor =
      skia::CGColorCreateFromSkColor(color).get();
}

void RenderWidgetHostNSViewBridge::SetVisible(bool visible) {
  ScopedCAActionDisabler disabler;
  cocoa_view_.hidden = !visible;
}

void RenderWidgetHostNSViewBridge::SetTooltipText(
    const std::u16string& tooltip_text) {
  // Called from the renderer to tell us what the tooltip text should be. It
  // calls us frequently so we need to cache the value to prevent doing a lot
  // of repeat work.
  if (tooltip_text == tooltip_text_ || !cocoa_view_.window.keyWindow) {
    return;
  }
  tooltip_text_ = tooltip_text;

  // Maximum number of characters we allow in a tooltip.
  const size_t kMaxTooltipLength = 1024;

  // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
  // Windows; we're just trying to be polite. Don't persist the trimmed
  // string, as then the comparison above will always fail and we'll try to
  // set it again every single time the mouse moves.
  std::u16string display_text = tooltip_text_;
  if (tooltip_text_.length() > kMaxTooltipLength)
    display_text = tooltip_text_.substr(0, kMaxTooltipLength);

  NSString* tooltip_nsstring = base::SysUTF16ToNSString(display_text);
  [cocoa_view_ setToolTipAtMousePoint:tooltip_nsstring];
}

void RenderWidgetHostNSViewBridge::SetCompositionRangeInfo(
    const gfx::Range& range) {
  [cocoa_view_ setCompositionRange:range];
  [cocoa_view_ setMarkedRange:range.ToNSRange()];
}

void RenderWidgetHostNSViewBridge::CancelComposition() {
  [cocoa_view_ cancelComposition];
}

void RenderWidgetHostNSViewBridge::SetTextInputState(
    ui::TextInputType text_input_type,
    uint32_t flags) {
  [cocoa_view_ setTextInputType:text_input_type];
  [cocoa_view_ setTextInputFlags:flags];
}

void RenderWidgetHostNSViewBridge::SetTextSelection(const std::u16string& text,
                                                    uint64_t offset,
                                                    const gfx::Range& range) {
  [cocoa_view_ setTextSelectionText:text offset:offset range:range];
  // Updates markedRange when there is no marked text so that retrieving
  // markedRange immediately after calling setMarkedText: returns the current
  // caret position.
  if (![cocoa_view_ hasMarkedText]) {
    [cocoa_view_ setMarkedRange:range.ToNSRange()];
  }
}

void RenderWidgetHostNSViewBridge::SetShowingContextMenu(bool showing) {
  [cocoa_view_ setShowingContextMenu:showing];
}

void RenderWidgetHostNSViewBridge::OnDisplayAdded(const display::Display&) {
  [cocoa_view_ updateScreenProperties];
}

void RenderWidgetHostNSViewBridge::OnDisplayRemoved(const display::Display&) {
  [cocoa_view_ updateScreenProperties];
}

void RenderWidgetHostNSViewBridge::OnDisplayMetricsChanged(
    const display::Display&,
    uint32_t) {
  // Note that -updateScreenProperties is also be called by the notifications
  // NSWindowDidChangeScreen and NSWindowDidChangeBackingPropertiesNotification,
  // so some of these calls will be redundant.
  [cocoa_view_ updateScreenProperties];
}

void RenderWidgetHostNSViewBridge::DisplayCursor(const ui::Cursor& cursor) {
  [cocoa_view_ updateCursor:ui::GetNativeCursor(cursor)];
}

void RenderWidgetHostNSViewBridge::SetCursorLocked(bool locked) {
  [cocoa_view_ setCursorLocked:locked];
}

void RenderWidgetHostNSViewBridge::SetCursorLockedUnacceleratedMovement(
    bool unaccelerated) {
  [cocoa_view_ setCursorLockedUnacceleratedMovement:unaccelerated];
}

void RenderWidgetHostNSViewBridge::ShowDictionaryOverlayForSelection() {
  NSRange selection_range = [cocoa_view_ selectedRange];
  [cocoa_view_ showLookUpDictionaryOverlayFromRange:selection_range];
}

void RenderWidgetHostNSViewBridge::ShowDictionaryOverlay(
    ui::mojom::AttributedStringPtr attributed_string,
    const gfx::Point& baseline_point) {
  CFAttributedStringRef cf_string =
      attributed_string.To<CFAttributedStringRef>();
  NSAttributedString* string = base::apple::CFToNSPtrCast(cf_string);
  if (string.length == 0) {
    return;
  }
  NSPoint flipped_baseline_point = {
      static_cast<CGFloat>(baseline_point.x()),
      cocoa_view_.frame.size.height - baseline_point.y(),
  };
  [cocoa_view_ showDefinitionForAttributedString:string
                                         atPoint:flipped_baseline_point];
}

void RenderWidgetHostNSViewBridge::LockKeyboard(
    const absl::optional<std::vector<uint32_t>>& uint_dom_codes) {
  absl::optional<base::flat_set<ui::DomCode>> dom_codes;
  if (uint_dom_codes) {
    dom_codes.emplace();
    for (const auto& uint_dom_code : *uint_dom_codes)
      dom_codes->insert(static_cast<ui::DomCode>(uint_dom_code));
  }
  [cocoa_view_ lockKeyboard:std::move(dom_codes)];
}

void RenderWidgetHostNSViewBridge::UnlockKeyboard() {
  [cocoa_view_ unlockKeyboard];
}

void RenderWidgetHostNSViewBridge::ShowSharingServicePicker(
    const std::string& title,
    const std::string& text,
    const std::string& url,
    const std::vector<std::string>& file_paths,
    ShowSharingServicePickerCallback callback) {
  ShowSharingServicePickerForView(cocoa_view_, title, text, url, file_paths,
                                  std::move(callback));
}

void RenderWidgetHostNSViewBridge::Destroy() {
  if (destroy_callback_)
    std::move(destroy_callback_).Run();
}

void RenderWidgetHostNSViewBridge::GestureScrollEventAck(
    std::unique_ptr<blink::WebCoalescedInputEvent> event,
    bool consumed) {
  if (!event ||
      !blink::WebInputEvent::IsGestureEventType(event->Event().GetType())) {
    DLOG(ERROR) << "Absent or non-GestureEventType event.";
    return;
  }

  const blink::WebGestureEvent& gesture_event =
      static_cast<const blink::WebGestureEvent&>(event->Event());
  [cocoa_view_ processedGestureScrollEvent:gesture_event consumed:consumed];
}

void RenderWidgetHostNSViewBridge::DidOverscroll(
    blink::mojom::DidOverscrollParamsPtr overscroll) {
  if (!overscroll) {
    DLOG(ERROR) << "Overscroll argument is nullptr.";
    return;
  }

  ui::DidOverscrollParams params = {
      overscroll->accumulated_overscroll, overscroll->latest_overscroll_delta,
      overscroll->current_fling_velocity,
      overscroll->causal_event_viewport_point, overscroll->overscroll_behavior};
  [cocoa_view_ processedOverscroll:params];
}

}  // namespace remote_cocoa
