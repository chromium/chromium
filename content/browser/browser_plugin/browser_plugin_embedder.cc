// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "base/bind.h"
#include "content/browser/bad_message.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/drag_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

BrowserPluginEmbedder::BrowserPluginEmbedder(WebContentsImpl* web_contents)
    : WebContentsObserver(web_contents), guest_drag_ending_(false) {}

BrowserPluginEmbedder::~BrowserPluginEmbedder() {
}

// static
BrowserPluginEmbedder* BrowserPluginEmbedder::Create(
    WebContentsImpl* web_contents) {
  return new BrowserPluginEmbedder(web_contents);
}

bool BrowserPluginEmbedder::DragEnteredGuest(BrowserPluginGuest* guest) {
  guest_dragging_over_ = guest->AsWeakPtr();
  return guest_started_drag_.get() == guest;
}

void BrowserPluginEmbedder::DragLeftGuest(BrowserPluginGuest* guest) {
  // Avoid race conditions in switching between guests being hovered over by
  // only un-setting if the caller is marked as the guest being dragged over.
  if (guest_dragging_over_.get() == guest) {
    guest_dragging_over_.reset();
  }
}

// static
bool BrowserPluginEmbedder::CancelDialogs(WebContents* guest_web_contents) {
  static_cast<WebContentsImpl*>(guest_web_contents)
      ->CancelActiveAndPendingDialogs();

  // Returns false to iterate over all guests.
  return false;
}

void BrowserPluginEmbedder::CancelGuestDialogs() {
  if (!GetBrowserPluginGuestManager())
    return;

  GetBrowserPluginGuestManager()->ForEachGuest(
      web_contents(),
      base::BindRepeating(&BrowserPluginEmbedder::CancelDialogs));
}

void BrowserPluginEmbedder::StartDrag(BrowserPluginGuest* guest) {
  guest_started_drag_ = guest->AsWeakPtr();
  guest_drag_ending_ = false;
}

BrowserPluginGuestManager*
BrowserPluginEmbedder::GetBrowserPluginGuestManager() const {
  return web_contents()->GetBrowserContext()->GetGuestManager();
}

void BrowserPluginEmbedder::ClearGuestDragStateIfApplicable() {
  // The order at which we observe SystemDragEnded() and DragSourceEndedAt() is
  // platform dependent.
  // In OSX, we see SystemDragEnded() first, where in aura, we see
  // DragSourceEndedAt() first. For this reason, we check if both methods were
  // called before resetting |guest_started_drag_|.
  if (guest_drag_ending_) {
    if (guest_started_drag_)
      guest_started_drag_.reset();
  } else {
    guest_drag_ending_ = true;
  }
}

// static
bool BrowserPluginEmbedder::DidSendScreenRectsCallback(
   WebContents* guest_web_contents) {
  static_cast<WebContentsImpl*>(guest_web_contents)->SendScreenRects();
  // Not handled => Iterate over all guests.
  return false;
}

void BrowserPluginEmbedder::DidSendScreenRects() {
  if (!GetBrowserPluginGuestManager())
    return;

  GetBrowserPluginGuestManager()->ForEachGuest(
      web_contents(),
      base::BindRepeating(&BrowserPluginEmbedder::DidSendScreenRectsCallback));
}

bool BrowserPluginEmbedder::OnMessageReceived(
    const IPC::Message& message,
    RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(BrowserPluginEmbedder, message,
                                   render_frame_host)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_Attach, OnAttach)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPluginEmbedder::DragSourceEndedAt(
    float client_x,
    float client_y,
    float screen_x,
    float screen_y,
    blink::WebDragOperation operation) {
  if (guest_started_drag_) {
    gfx::Point guest_offset =
        guest_started_drag_->GetScreenCoordinates(gfx::Point());
    guest_started_drag_->DragSourceEndedAt(client_x - guest_offset.x(),
        client_y - guest_offset.y(), screen_x, screen_y, operation);
  }
  ClearGuestDragStateIfApplicable();
}

void BrowserPluginEmbedder::SystemDragEnded() {
  // When the embedder's drag/drop operation ends, we need to pass the message
  // to the guest that initiated the drag/drop operation. This will ensure that
  // the guest's RVH state is reset properly.
  if (guest_started_drag_)
    guest_started_drag_->EmbedderSystemDragEnded();

  guest_dragging_over_.reset();
  ClearGuestDragStateIfApplicable();
}

bool BrowserPluginEmbedder::OnUpdateDragCursor() {
  return !!guest_dragging_over_;
}

void BrowserPluginEmbedder::OnAttach(
    RenderFrameHost* render_frame_host,
    int browser_plugin_instance_id,
    const BrowserPluginHostMsg_Attach_Params& params) {
  if (!GetBrowserPluginGuestManager()) {
    bad_message::ReceivedBadMessage(
        render_frame_host->GetProcess(),
        bad_message::BPE_UNEXPECTED_MESSAGE_BEFORE_BPGM_CREATION);
    return;
  }
  WebContents* guest_web_contents =
      GetBrowserPluginGuestManager()->GetGuestByInstanceID(
          render_frame_host->GetProcess()->GetID(),
          browser_plugin_instance_id);
  if (!guest_web_contents)
    return;
  BrowserPluginGuest* guest = static_cast<WebContentsImpl*>(guest_web_contents)
                                  ->GetBrowserPluginGuest();
  guest->Attach(browser_plugin_instance_id,
                static_cast<WebContentsImpl*>(web_contents()),
                params);
}

bool BrowserPluginEmbedder::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if ((event.windows_key_code != ui::VKEY_ESCAPE) ||
      (event.GetModifiers() & blink::WebInputEvent::kInputModifiers)) {
    return false;
  }

  bool event_consumed = false;
  GetBrowserPluginGuestManager()->ForEachGuest(
      web_contents(),
      base::BindRepeating(
          &BrowserPluginEmbedder::UnlockMouseIfNecessaryCallback,
          &event_consumed));

  return event_consumed;
}

BrowserPluginGuest* BrowserPluginEmbedder::GetFullPageGuest() {
  WebContentsImpl* guest_contents = static_cast<WebContentsImpl*>(
      GetBrowserPluginGuestManager()->GetFullPageGuest(web_contents()));
  if (!guest_contents)
    return nullptr;
  return guest_contents->GetBrowserPluginGuest();
}

// static
bool BrowserPluginEmbedder::GuestCurrentlyAudibleCallback(WebContents* guest) {
  return guest->IsCurrentlyAudible();
}

bool BrowserPluginEmbedder::AreAnyGuestsCurrentlyAudible() {
  if (!GetBrowserPluginGuestManager())
    return false;

  return GetBrowserPluginGuestManager()->ForEachGuest(
      web_contents(),
      base::BindRepeating(
          &BrowserPluginEmbedder::GuestCurrentlyAudibleCallback));
}

// static
bool BrowserPluginEmbedder::UnlockMouseIfNecessaryCallback(bool* mouse_unlocked,
                                                           WebContents* guest) {
  *mouse_unlocked |= static_cast<WebContentsImpl*>(guest)
                         ->GetBrowserPluginGuest()
                         ->mouse_locked();
  guest->GotResponseToLockMouseRequest(false);

  // Returns false to iterate over all guests.
  return false;
}

}  // namespace content
