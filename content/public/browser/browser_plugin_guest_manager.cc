// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_plugin_guest_manager.h"

namespace content {

bool BrowserPluginGuestManager::ForEachGuest(
    WebContents* owner_web_contents,
    base::FunctionRef<bool(WebContents*)> fn) {
  return false;
}

WebContents* BrowserPluginGuestManager::GetFullPageGuest(
    WebContents* embedder_web_contents) {
  return nullptr;
}

}  // namespace content
