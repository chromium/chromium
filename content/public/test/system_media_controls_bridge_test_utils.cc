// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/system_media_controls_bridge_test_utils.h"

#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_keys_listener_manager_impl.h"
#include "content/browser/media/web_app_system_media_controls_manager.h"

namespace content {

void SetOnSystemMediaControlsBridgeCreatedCallbackForTesting(
    base::RepeatingCallback<void()> callback) {
  BrowserMainLoop::GetInstance()
      ->media_keys_listener_manager()
      ->web_app_system_media_controls_manager_for_testing()
      ->SetOnSystemMediaControlsBridgeCreatedCallbackForTesting(
          std::move(callback));
}

void SetOnBrowserSystemMediaControlsBridgeCreatedCallbackForTesting(
    base::RepeatingCallback<void()> callback) {
  BrowserMainLoop::GetInstance()
      ->media_keys_listener_manager()
      ->SetOnSystemMediaControlsBridgeCreatedCallbackForTesting(
          std::move(callback));
}

}  // namespace content
