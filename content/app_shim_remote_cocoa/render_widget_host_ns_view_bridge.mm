// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/render_widget_host_ns_view_bridge.h"

#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#include "content/app_shim_remote_cocoa/render_widget_host_ns_view_host_helper.h"
#include "content/common/cursors/webcursor.h"
#import "skia/ext/skia_utils_mac.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#import "ui/base/cocoa/animation_utils.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/mac/coordinate_conversion.h"

namespace remote_cocoa {

RenderWidgetHostNSViewBridge::RenderWidgetHostNSViewBridge(
    mojom::RenderWidgetHostNSViewHost* host,
    RenderWidgetHostNSViewHostHelper* host_helper) {
  display::Screen::GetScreen()->AddObserver(this);

  cocoa_view_.reset([[RenderWidgetHostViewCocoa alloc]
        initWithHost:host
      withHostHelper:host_helper]);

  background_layer_.reset([[CALayer alloc] init]);
  display_ca_layer_tree_ =
      std::make_unique<ui::DisplayCALayerTree>(background_layer_.get());
  [cocoa_view_ setLayer:background_layer_];
  [cocoa_view_ setWantsLayer:YES];
}

RenderWidgetHostNSViewBridge::~RenderWidgetHostNSViewBridge() {
  [cocoa_view_ setHostDisconnected];
  // Do not immediately remove |cocoa_view_| from the NSView heirarchy, because
  // the call to -[NSView removeFromSuperview] may cause use to call into the
  // RWHVMac during tear-down, via WebContentsImpl::UpdateWebContentsVisibility.
  // https://crbug.com/834931
  [cocoa_view_ performSelector:@selector(removeFromSuperview)
                    withObject:nil
                    afterDelay:0];
  cocoa_view_.autorelease();
  display::Screen::GetScreen()->RemoveObserver(this);
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

void RenderWidgetHostNSViewBridge::InitAsPopup(const gfx::Rect& content_rect) {
  popup_window_ = std::make_unique<PopupWindowMac>(content_rect, cocoa_view_);
}

void RenderWidgetHostNSViewBridge::SetParentWebContentsNSView(
    uint64_t parent_ns_view_id) {
  NSView* parent_ns_view = remote_cocoa::GetNSViewFromId(parent_ns_view_id);
  // If the browser passed an invalid handle, then there is no recovery.
  CHECK(parent_ns_view);
  // Set the frame and autoresizing mask of the RenderWidgetHostViewCocoa as is
  // done by WebContentsViewMac.
  [cocoa_view_ setFrame:[parent_ns_view bounds]];
  [cocoa_view_ setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  // Place the new view below all other views, matching the behavior in
  // WebContentsViewMac::CreateViewForWidget.
  // https://crbug.com/1017446
  [parent_ns_view addSubview:cocoa_view_
                  positioned:NSWindowBelow
                  relativeTo:nil];
}

void RenderWidgetHostNSViewBridge::MakeFirstResponder() {
  [[cocoa_view_ window] makeFirstResponder:cocoa_view_];
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
      IsPopup() || ![[cocoa_view_ superview] isKindOfClass:[BaseView class]];
  if (isRelativeToScreen) {
    // The position of |rect| is screen coordinate system and we have to
    // consider Cocoa coordinate system is upside-down and also multi-screen.
    NSRect frame = gfx::ScreenRectToNSRect(rect);
    if (IsPopup())
      [popup_window_->window() setFrame:frame display:YES];
    else
      [cocoa_view_ setFrame:frame];
  } else {
    BaseView* superview = static_cast<BaseView*>([cocoa_view_ superview]);
    gfx::Rect rect2 = [superview flipNSRectToRect:[cocoa_view_ frame]];
    rect2.set_width(rect.width());
    rect2.set_height(rect.height());
    [cocoa_view_ setFrame:[superview flipRectToNSRect:rect2]];
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
  base::ScopedCFTypeRef<CGColorRef> cg_color(
      skia::CGColorCreateFromSkColor(color));
  [background_layer_ setBackgroundColor:cg_color];
}

void RenderWidgetHostNSViewBridge::SetVisible(bool visible) {
  ScopedCAActionDisabler disabler;
  [cocoa_view_ setHidden:!visible];
}

void RenderWidgetHostNSViewBridge::SetTooltipText(
    const base::string16& tooltip_text) {
  // Called from the renderer to tell us what the tooltip text should be. It
  // calls us frequently so we need to cache the value to prevent doing a lot
  // of repeat work.
  if (tooltip_text == tooltip_text_ || ![[cocoa_view_ window] isKeyWindow])
    return;
  tooltip_text_ = tooltip_text;

  // Maximum number of characters we allow in a tooltip.
  const size_t kMaxTooltipLength = 1024;

  // Clamp the tooltip length to kMaxTooltipLength. It's a DOS issue on
  // Windows; we're just trying to be polite. Don't persist the trimmed
  // string, as then the comparison above will always fail and we'll try to
  // set it again every single time the mouse moves.
  base::string16 display_text = tooltip_text_;
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

void RenderWidgetHostNSViewBridge::SetTextSelection(const base::string16& text,
                                                    uint64_t offset,
                                                    const gfx::Range& range) {
  [cocoa_view_ setTextSelectionText:text offset:offset range:range];
  // Updates markedRange when there is no marked text so that retrieving
  // markedRange immediately after calling setMarkdText: returns the current
  // caret position.
  if (![cocoa_view_ hasMarkedText]) {
    [cocoa_view_ setMarkedRange:range.ToNSRange()];
  }
}

void RenderWidgetHostNSViewBridge::SetShowingContextMenu(bool showing) {
  [cocoa_view_ setShowingContextMenu:showing];
}

void RenderWidgetHostNSViewBridge::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // Note that -updateScreenProperties is also be called by the notification
  // NSWindowDidChangeBackingPropertiesNotification (some of these calls
  // will be redundant).
  [cocoa_view_ updateScreenProperties];
}

void RenderWidgetHostNSViewBridge::DisplayCursor(
    const content::WebCursor& cursor) {
  content::WebCursor non_const_cursor(cursor);
  [cocoa_view_ updateCursor:non_const_cursor.GetNativeCursor()];
}

void RenderWidgetHostNSViewBridge::SetCursorLocked(bool locked) {
  [cocoa_view_ setCursorLocked:locked];
}

void RenderWidgetHostNSViewBridge::ShowDictionaryOverlayForSelection() {
  NSRange selection_range = [cocoa_view_ selectedRange];
  [cocoa_view_ showLookUpDictionaryOverlayFromRange:selection_range];
}

void RenderWidgetHostNSViewBridge::ShowDictionaryOverlay(
    const mac::AttributedStringCoder::EncodedString& encoded_string,
    const gfx::Point& baseline_point) {
  NSAttributedString* string =
      mac::AttributedStringCoder::Decode(&encoded_string);
  if ([string length] == 0)
    return;
  NSPoint flipped_baseline_point = {
      baseline_point.x(),
      [cocoa_view_ frame].size.height - baseline_point.y(),
  };
  [cocoa_view_ showDefinitionForAttributedString:string
                                         atPoint:flipped_baseline_point];
}

void RenderWidgetHostNSViewBridge::LockKeyboard(
    const base::Optional<std::vector<uint32_t>>& uint_dom_codes) {
  base::Optional<base::flat_set<ui::DomCode>> dom_codes;
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

}  // namespace remote_cocoa
