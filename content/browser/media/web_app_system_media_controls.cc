// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/web_app_system_media_controls.h"

namespace content {

using system_media_controls::SystemMediaControls;

WebAppSystemMediaControls::WebAppSystemMediaControls(
    base::UnguessableToken request_id,
    std::unique_ptr<system_media_controls::SystemMediaControls>
        system_media_controls,
    std::unique_ptr<SystemMediaControlsNotifier> notifier,
    std::unique_ptr<ActiveMediaSessionController> controller)
    : request_id_(request_id),
      system_media_controls_(std::move(system_media_controls)),
      notifier_(std::move(notifier)),
      controller_(std::move(controller)) {}

WebAppSystemMediaControls::WebAppSystemMediaControls() = default;

WebAppSystemMediaControls::~WebAppSystemMediaControls() = default;

SystemMediaControls* WebAppSystemMediaControls::GetSystemMediaControls() const {
  return system_media_controls_.get();
}

SystemMediaControlsNotifier* WebAppSystemMediaControls::GetNotifier() const {
  return notifier_.get();
}

ActiveMediaSessionController* WebAppSystemMediaControls::GetController() const {
  return controller_.get();
}

}  // namespace content
