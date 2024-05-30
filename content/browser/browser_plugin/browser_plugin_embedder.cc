// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_plugin/browser_plugin_embedder.h"

#include "base/functional/bind.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {

BrowserPluginEmbedder::BrowserPluginEmbedder(WebContentsImpl* web_contents)
    : web_contents_(web_contents) {}

BrowserPluginEmbedder::~BrowserPluginEmbedder() = default;

// static
BrowserPluginEmbedder* BrowserPluginEmbedder::Create(
    WebContentsImpl* web_contents) {
  return new BrowserPluginEmbedder(web_contents);
}

void BrowserPluginEmbedder::CancelGuestDialogs() {
  if (!GetBrowserPluginGuestManager())
    return;

  GetBrowserPluginGuestManager()->ForEachGuest(
      web_contents_, [](WebContents* guest_web_contents) {
        static_cast<WebContentsImpl*>(guest_web_contents)
            ->CancelActiveAndPendingDialogs();
        // Returns false to iterate over all guests.
        return false;
      });
}

BrowserPluginGuestManager*
BrowserPluginEmbedder::GetBrowserPluginGuestManager() const {
  return web_contents_->GetBrowserContext()->GetGuestManager();
}

bool BrowserPluginEmbedder::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  if ((event.windows_key_code != ui::VKEY_ESCAPE) ||
      (event.GetModifiers() & blink::WebInputEvent::kInputModifiers)) {
    return false;
  }

  GetBrowserPluginGuestManager()->ForEachGuest(
      web_contents_, [](WebContents* guest) {
        guest->GotResponseToPointerLockRequest(
            blink::mojom::PointerLockResult::kUserRejected);

        // Returns false to iterate over all guests.
        return false;
      });

  return false;
}

BrowserPluginGuest* BrowserPluginEmbedder::GetFullPageGuest() {
  WebContentsImpl* guest_contents = static_cast<WebContentsImpl*>(
      GetBrowserPluginGuestManager()->GetFullPageGuest(web_contents_));
  if (!guest_contents)
    return nullptr;
  return guest_contents->GetBrowserPluginGuest();
}

bool BrowserPluginEmbedder::AreAnyGuestsCurrentlyAudible() {
  if (!GetBrowserPluginGuestManager())
    return false;

  return GetBrowserPluginGuestManager()->ForEachGuest(
      web_contents_, &WebContents::IsCurrentlyAudible);
}

}  // namespace content
