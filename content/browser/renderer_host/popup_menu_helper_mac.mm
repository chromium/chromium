// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/popup_menu_helper_mac.h"

#import "base/mac/scoped_sending_event.h"
#import "base/message_loop/message_pump_apple.h"
#include "base/task/current_thread.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/web_menu_runner_mac.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/base_view.h"

namespace content {

namespace {

bool g_allow_showing_popup_menus = true;

}  // namespace

struct PopupMenuHelper::ObjCStorage {
  WebMenuRunner* __weak menu_runner;
};

PopupMenuHelper::PopupMenuHelper(
    Delegate* delegate,
    RenderFrameHost* render_frame_host,
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client)
    : delegate_(delegate),
      render_frame_host_(
          static_cast<RenderFrameHostImpl*>(render_frame_host)->GetWeakPtr()),
      popup_client_(std::move(popup_client)),
      objc_storage_(std::make_unique<ObjCStorage>()) {
  RenderWidgetHost* widget_host =
      render_frame_host->GetRenderViewHost()->GetWidget();
  observation_.Observe(widget_host);

  popup_client_.set_disconnect_handler(
      base::BindOnce(&PopupMenuHelper::Hide, weak_ptr_factory_.GetWeakPtr()));
}

PopupMenuHelper::~PopupMenuHelper() {
  Hide();
}

void PopupMenuHelper::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    std::vector<blink::mojom::MenuItemPtr> items,
    bool right_aligned,
    bool allow_multiple_selection) {
  // Only single selection list boxes show a popup on Mac.
  DCHECK(!allow_multiple_selection);

  if (!g_allow_showing_popup_menus)
    return;

  RenderWidgetHostViewMac* rwhvm = GetRenderWidgetHostView();
  auto* web_contents = rwhvm->GetWebContents();

  // Convert element_bounds to be in screen.
  gfx::Rect client_area = web_contents->GetContainerBounds();
  gfx::Rect bounds_in_screen = bounds + client_area.OffsetFromOrigin();

  // The new popup menu would overlap the permission prompt, which could lead to
  // users making decisions based on incorrect information. We should close the
  // popup if it intersects with the permission prompt.
  auto permission_exclusion_area_bounds =
      PermissionControllerImpl::FromBrowserContext(
          web_contents->GetBrowserContext())
          ->GetExclusionAreaBoundsInScreen(web_contents);
  if (permission_exclusion_area_bounds &&
      permission_exclusion_area_bounds->Intersects(bounds_in_screen)) {
    popup_client_->DidCancel();
    delegate_->OnMenuClosed();  // May delete |this|.
    return;
  }

  // Retain the Cocoa view for the duration of the pop-up so that it can't be
  // dealloced if my Destroy() method is called while the pop-up's up (which
  // would in turn delete me, causing a crash once the -runMenuInView
  // call returns. That's what was happening in <http://crbug.com/33250>).
  RenderWidgetHostViewCocoa* cocoa_view = rwhvm->GetInProcessNSView();

  // Check if the underlying native window is headless and if so, return early
  // to avoid showing the popup menu.
  NativeWidgetMacNSWindow* ns_window =
      base::apple::ObjCCastStrict<NativeWidgetMacNSWindow>([cocoa_view window]);
  if (ns_window && [ns_window isHeadless]) {
    return;
  }

  // Display the menu.
  WebMenuRunner* runner = [[WebMenuRunner alloc] initWithItems:items
                                                      fontSize:item_font_size
                                                  rightAligned:right_aligned];

  // Take a weak reference so that Hide() can close the menu.
  objc_storage_->menu_runner = runner;

  base::WeakPtr<PopupMenuHelper> weak_ptr(weak_ptr_factory_.GetWeakPtr());

  {
    // Make sure events can be pumped while the menu is up.
    base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;

    // One of the events that could be pumped is |window.close()|.
    // User-initiated event-tracking loops protect against this by
    // setting flags in -[CrApplication sendEvent:], but since
    // web-content menus are initiated by IPC message the setup has to
    // be done manually.
    base::mac::ScopedSendingEvent sending_event_scoper;

    // Ensure the UI can update while the menu is fading out.
    pump_in_fade_ = std::make_unique<base::ScopedPumpMessagesInPrivateModes>();

    // Now run a NESTED EVENT LOOP until the pop-up is finished.
    [runner runMenuInView:cocoa_view
               withBounds:[cocoa_view flipRectToNSRect:bounds]
             initialIndex:selected_item];
  }

  if (!weak_ptr)
    return;  // Handle |this| being deleted.

  pump_in_fade_ = nullptr;
  objc_storage_->menu_runner = nil;

  // The RenderFrameHost may be deleted while running the menu, or it may have
  // requested the close. Don't notify in these cases.
  if (popup_client_ && !popup_was_hidden_) {
    if ([runner menuItemWasChosen]) {
      int index = [runner indexOfSelectedItem];
      if (index < 0)
        popup_client_->DidCancel();
      else
        popup_client_->DidAcceptIndices({index});
    } else {
      popup_client_->DidCancel();
    }
  }

  delegate_->OnMenuClosed();  // May delete |this|.
}

void PopupMenuHelper::Hide() {
  // FYI: Blink reuses the PopupMenu of an element and first invokes Hide() over
  // IPC if a menu is already showing. Attempting to show a new menu while the
  // old menu is fading out confuses AppKit, since we're still in the NESTED
  // EVENT LOOP of ShowPopupMenu(). That is why WebMenuRunner has to provide a
  // synchronous, no-animation cancellation. See https://crbug.com/812260.
  if (objc_storage_->menu_runner) {
    [objc_storage_->menu_runner cancelSynchronously];
  }
  popup_was_hidden_ = true;
  popup_client_.reset();
}

// static
void PopupMenuHelper::DontShowPopupMenuForTesting() {
  g_allow_showing_popup_menus = false;
}

RenderWidgetHostViewMac* PopupMenuHelper::GetRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewMac*>(
      render_frame_host_->GetOutermostMainFrameOrEmbedder()->GetView());
}

void PopupMenuHelper::RenderWidgetHostVisibilityChanged(
    RenderWidgetHost* widget_host,
    bool became_visible) {
  if (!became_visible)
    Hide();
}

void PopupMenuHelper::RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) {
  DCHECK(observation_.IsObservingSource(widget_host));
  observation_.Reset();
}

}  // namespace content
