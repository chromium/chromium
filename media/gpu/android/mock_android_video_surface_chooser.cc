// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/mock_android_video_surface_chooser.h"

namespace media {

MockAndroidVideoSurfaceChooser::MockAndroidVideoSurfaceChooser() = default;
MockAndroidVideoSurfaceChooser::~MockAndroidVideoSurfaceChooser() = default;

void MockAndroidVideoSurfaceChooser::SetClientCallbacks(
    UseOverlayCB use_overlay_cb,
    UseTextureOwnerCB use_texture_owner_cb) {
  MockSetClientCallbacks();
  use_overlay_cb_ = std::move(use_overlay_cb);
  use_texture_owner_cb_ = std::move(use_texture_owner_cb);
}

void MockAndroidVideoSurfaceChooser::UpdateState(
    std::optional<AndroidOverlayFactoryCB> factory,
    const State& new_state) {
  MockUpdateState();
  if (factory) {
    factory_ = std::move(*factory);
    MockReplaceOverlayFactory(!factory_.is_null());
  }
  current_state_ = new_state;
}

void MockAndroidVideoSurfaceChooser::ProvideTextureOwner() {
  use_texture_owner_cb_.Run();
}

void MockAndroidVideoSurfaceChooser::ProvideOverlay(
    std::unique_ptr<AndroidOverlay> overlay) {
  use_overlay_cb_.Run(std::move(overlay));
}

}  // namespace media
