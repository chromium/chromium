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
    RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
    CloseListenerHost* current_host =
        focused_frame ? CloseListenerHost::GetForCurrentDocument(focused_frame)
                      : nullptr;
    if (current_host) {
      manager->MaybeUpdateInterceptStatus(current_host);
    }
  }
}

void CloseListenerManager::MaybeUpdateInterceptStatus(
    CloseListenerHost* host_being_updated) {
  if (GetWebContents().IsBeingDestroyed()) {
    return;
  }
  if (&host_being_updated->render_frame_host() !=
      GetWebContents().GetFocusedFrame()) {
    return;
  }
  bool should_intercept = host_being_updated->IsActive();
  if (should_intercept == should_intercept_) {
    return;
  }
  should_intercept_ = should_intercept;
  GetWebContents().GetDelegate()->DidChangeCloseSignalInterceptStatus();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CloseListenerManager);

}  // namespace content
