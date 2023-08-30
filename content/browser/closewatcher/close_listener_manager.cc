// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/closewatcher/close_listener_manager.h"

#include "content/browser/closewatcher/close_listener_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

CloseListenerManager::CloseListenerManager(WebContents* web_contents)
    : WebContentsUserData<CloseListenerManager>(*web_contents) {}

CloseListenerManager::~CloseListenerManager() = default;

// static
void CloseListenerManager::DidChangeFocusedFrame(WebContents* web_contents) {
  if (auto* manager = CloseListenerManager::FromWebContents(web_contents)) {
    manager->UpdateInterceptStatus();
  }
}

void CloseListenerManager::UpdateInterceptStatus() {
  if (GetWebContents().IsBeingDestroyed()) {
    return;
  }
  RenderFrameHost* focused_frame = GetWebContents().GetFocusedFrame();
  CloseListenerHost* current_host =
      focused_frame ? CloseListenerHost::GetForCurrentDocument(focused_frame)
                    : nullptr;
  bool should_intercept = current_host && current_host->IsActive();
  if (should_intercept == should_intercept_) {
    return;
  }
  should_intercept_ = should_intercept;
  GetWebContents().GetDelegate()->DidChangeCloseSignalInterceptStatus();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CloseListenerManager);

}  // namespace content
