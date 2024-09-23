// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/render_widget_host_ns_view_bridge.h"

#include <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#import "base/mac/scoped_sending_event.h"
#import "base/message_loop/message_pump_apple.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#include "content/app_shim_remote_cocoa/render_widget_host_ns_view_host_helper.h"
#import "content/app_shim_remote_cocoa/web_menu_runner_mac.h"
#include "content/common/mac/attributed_string_type_converters.h"
#import "skia/ext/skia_utils_mac.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
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
  // Make the initial view visibility state in sync with that of
  // `RenderWidgetHostViewMac::is_visible_`, which is false.
  cocoa_view_.hidden = true;

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

void RenderWidgetHostNSViewBridge::OnDisplaysRemoved(const display::Displays&) {
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
    const std::optional<std::vector<uint32_t>>& uint_dom_codes) {
  std::optional<base::flat_set<ui::DomCode>> dom_codes;
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
  NSString* ns_title = base::SysUTF8ToNSString(title);
  NSString* ns_url = base::SysUTF8ToNSString(url);
  NSString* ns_text = base::SysUTF8ToNSString(text);

  NSMutableArray* items = [@[ ns_title, ns_url, ns_text ] mutableCopy];

  for (const auto& file_path : file_paths) {
    NSString* ns_file_path = base::SysUTF8ToNSString(file_path);
    NSURL* file_url = [NSURL fileURLWithPath:ns_file_path];
    [items addObject:file_url];
  }

  sharing_service_picker_ = [[SharingServicePicker alloc]
      initWithItems:items
           callback:base::BindOnce(
                        &RenderWidgetHostNSViewBridge::OnSharingServiceInvoked,
                        weak_factory_.GetWeakPtr(), std::move(callback))
               view:cocoa_view_];
  [sharing_service_picker_ show];
}

void RenderWidgetHostNSViewBridge::OnSharingServiceInvoked(
    ShowSharingServicePickerCallback callback,
    blink::mojom::ShareError error) {
  std::move(callback).Run(error);
  sharing_service_picker_ = nil;
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

namespace {
class PopupMenuRunner : public mojom::PopupMenuRunner {
 public:
  PopupMenuRunner(mojo::PendingReceiver<mojom::PopupMenuRunner> receiver,
                  WebMenuRunner* runner)
      : receiver_(this, std::move(receiver)), menu_runner_(runner) {}

  void Hide() override {
    if (menu_runner_) {
      [menu_runner_ cancelSynchronously];
    }
  }

 private:
  mojo::Receiver<mojom::PopupMenuRunner> receiver_;
  WebMenuRunner* __weak menu_runner_;
};
}  // namespace

void RenderWidgetHostNSViewBridge::DisplayPopupMenu(
    mojom::PopupMenuPtr menu,
    DisplayPopupMenuCallback callback) {
  if (showing_popup_menu_) {
    // If we're currently showing a popup menu, we'll need to wait for that
    // menu to finish showing to get the nested run loop of the stack.
    // Attempting to show a new menu while the old menu is still visible or
    // fading out confuses AppKit, since we're still in the nested event loop of
    // DisplayPopupMenu(). See https://crbug.com/812260.
    pending_menus_.emplace_back(std::move(menu), std::move(callback));
    return;
  }

  // Check if the underlying native window is headless and if so, return early
  // to avoid showing the popup menu. In content_shell, the window is not a
  // `NativeWidgetMacNSWindow`, so this doesn't use a strict cast.
  NativeWidgetMacNSWindow* ns_window =
      base::apple::ObjCCast<NativeWidgetMacNSWindow>(cocoa_view_.window);
  if (ns_window && ns_window.isHeadless) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Retain the Cocoa view for the duration of the pop-up so that it can't be
  // dealloced if the widget is destroyed while the pop-up's up (which
  // would in turn delete me, causing a crash once the -runMenuInView
  // call returns. That's what was happening in <http://crbug.com/33250>).
  RenderWidgetHostViewCocoa* cocoa_view = cocoa_view_;

  // Get a weak pointer to `this`, so we can detect if we get destroyed while
  // in the nested event loop below.
  auto weak_self = weak_factory_.GetWeakPtr();

  WebMenuRunner* runner =
      [[WebMenuRunner alloc] initWithItems:menu->items
                                  fontSize:menu->item_font_size
                              rightAligned:menu->right_aligned];

  {
    // We can't use base::AutoReset to set and reset `showing_popup_menu_` as
    // `this` might be destroyed by the time showing the menu finishes.
    showing_popup_menu_ = true;
    absl::Cleanup running([weak_self]() {
      if (weak_self) {
        weak_self->showing_popup_menu_ = false;
      }
    });

    PopupMenuRunner mojo_host(std::move(menu->receiver), runner);

    // Make sure events can be pumped while the menu is up. But not when the
    // menu is being cancelled.
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop
        nested_allow;

    // Prevent an autorelease pool from being created in nested event loops.
    // Additionally, if this code runs in the browser process, one of the events
    // that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    base::mac::ScopedSendingEvent sending_event_scoper;

    // Ensure the UI can update while the menu is fading out.
    base::ScopedPumpMessagesInPrivateModes pump_in_fade;

    // Now run a NESTED EVENT LOOP until the pop-up is finished.
    [runner runMenuInView:cocoa_view
               withBounds:[cocoa_view flipRectToNSRect:menu->bounds]
             initialIndex:menu->selected_item];
  }

  if (!weak_self) {
    return;
  }

  if (runner.menuItemWasChosen) {
    int index = runner.indexOfSelectedItem;
    if (index < 0) {
      std::move(callback).Run(std::nullopt);
    } else {
      std::move(callback).Run(index);
    }
  } else {
    std::move(callback).Run(std::nullopt);
  }

  std::vector<PendingPopupMenu> next_menus = std::exchange(pending_menus_, {});
  if (!next_menus.empty()) {
    // If any DisplayPopupMenu calls came in while this one was showing, cancel
    // all but the last call and display the menu for the most recent call.
    for (int i = 0; i < static_cast<int>(next_menus.size()) - 1; ++i) {
      std::move(next_menus[i].second).Run(std::nullopt);
    }
    DisplayPopupMenu(std::move(next_menus.back().first),
                     std::move(next_menus.back().second));
  }
}

}  // namespace remote_cocoa
