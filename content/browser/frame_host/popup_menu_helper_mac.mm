// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/popup_menu_helper_mac.h"

#import "base/mac/scoped_nsobject.h"
#import "base/mac/scoped_sending_event.h"
#include "base/message_loop/message_loop_current.h"
#import "base/message_loop/message_pump_mac.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/browser/renderer_host/webmenurunner_mac.h"
#import "ui/base/cocoa/base_view.h"

namespace content {

namespace {

bool g_allow_showing_popup_menus = true;

}  // namespace

PopupMenuHelper::PopupMenuHelper(Delegate* delegate,
                                 RenderFrameHost* render_frame_host)
    : delegate_(delegate),
      render_frame_host_(
          static_cast<RenderFrameHostImpl*>(render_frame_host)->GetWeakPtr()) {
  RenderWidgetHost* widget_host =
      render_frame_host->GetRenderViewHost()->GetWidget();
  observer_.Add(widget_host);
}

PopupMenuHelper::~PopupMenuHelper() {
  Hide();
}

void PopupMenuHelper::ShowPopupMenu(
    const gfx::Rect& bounds,
    int item_height,
    double item_font_size,
    int selected_item,
    const std::vector<MenuItem>& items,
    bool right_aligned,
    bool allow_multiple_selection) {
  // Only single selection list boxes show a popup on Mac.
  DCHECK(!allow_multiple_selection);

  if (!g_allow_showing_popup_menus)
    return;

  // Retain the Cocoa view for the duration of the pop-up so that it can't be
  // dealloced if my Destroy() method is called while the pop-up's up (which
  // would in turn delete me, causing a crash once the -runMenuInView
  // call returns. That's what was happening in <http://crbug.com/33250>).
  RenderWidgetHostViewMac* rwhvm =
      static_cast<RenderWidgetHostViewMac*>(GetRenderWidgetHostView());
  base::scoped_nsobject<RenderWidgetHostViewCocoa> cocoa_view(
      [rwhvm->GetInProcessNSView() retain]);

  // Display the menu.
  base::scoped_nsobject<WebMenuRunner> runner([[WebMenuRunner alloc]
      initWithItems:items
           fontSize:item_font_size
       rightAligned:right_aligned]);

  // Take a weak reference so that Hide() can close the menu.
  menu_runner_ = runner;

  base::WeakPtr<PopupMenuHelper> weak_ptr(weak_ptr_factory_.GetWeakPtr());

  {
    // Make sure events can be pumped while the menu is up.
    base::MessageLoopCurrent::ScopedNestableTaskAllower allow;

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
  menu_runner_ = nil;

  // The RenderFrameHost may be deleted while running the menu, or it may have
  // requested the close. Don't notify in these cases.
  if (render_frame_host_ && !popup_was_hidden_) {
    if ([runner menuItemWasChosen])
      render_frame_host_->DidSelectPopupMenuItem([runner indexOfSelectedItem]);
    else
      render_frame_host_->DidCancelPopupMenu();
  }

  delegate_->OnMenuClosed();  // May delete |this|.
}

void PopupMenuHelper::Hide() {
  // Blink core reuses the PopupMenu of an element and first invokes Hide() over
  // IPC if a menu is already showing. Attempting to show a new menu while the
  // old menu is fading out confuses AppKit, since we're still in the NESTED
  // EVENT LOOP of ShowPopupMenu(). Disable pumping of events in the fade
  // animation of the old menu in this case so that it closes synchronously.
  // See http://crbug.com/812260.
  pump_in_fade_ = nullptr;

  if (menu_runner_)
    [menu_runner_ hide];
  popup_was_hidden_ = true;
}

// static
void PopupMenuHelper::DontShowPopupMenuForTesting() {
  g_allow_showing_popup_menus = false;
}

RenderWidgetHostViewMac* PopupMenuHelper::GetRenderWidgetHostView() const {
  return static_cast<RenderWidgetHostViewMac*>(
      render_frame_host_->frame_tree_node()
          ->frame_tree()
          ->root()
          ->current_frame_host()
          ->GetView());
}

void PopupMenuHelper::RenderWidgetHostVisibilityChanged(
    RenderWidgetHost* widget_host,
    bool became_visible) {
  if (!became_visible)
    Hide();
}

void PopupMenuHelper::RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) {
  observer_.Remove(widget_host);
}

}  // namespace content
