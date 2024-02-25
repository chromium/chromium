// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MOCK_ANDROID_VIDEO_SURFACE_CHOOSER_H_
#define MEDIA_GPU_ANDROID_MOCK_ANDROID_VIDEO_SURFACE_CHOOSER_H_

#include "media/gpu/android/android_video_surface_chooser.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// A mock surface chooser that lets tests choose the surface with
// ProvideOverlay() and ProvideTextureOwner().
class MockAndroidVideoSurfaceChooser : public AndroidVideoSurfaceChooser {
 public:
  MockAndroidVideoSurfaceChooser();

  MockAndroidVideoSurfaceChooser(const MockAndroidVideoSurfaceChooser&) =
      delete;
  MockAndroidVideoSurfaceChooser& operator=(
      const MockAndroidVideoSurfaceChooser&) = delete;

  ~MockAndroidVideoSurfaceChooser() override;

  // Mocks that are called by the fakes below.
  MOCK_METHOD0(MockSetClientCallbacks, void());
  MOCK_METHOD0(MockUpdateState, void());

  // Called by UpdateState if the factory is changed.  It is called with true if
  // and only if the replacement factory isn't null.
  MOCK_METHOD1(MockReplaceOverlayFactory, void(bool));

  void SetClientCallbacks(UseOverlayCB use_overlay_cb,
                          UseTextureOwnerCB use_texture_owner_cb) override;
  void UpdateState(std::optional<AndroidOverlayFactoryCB> factory,
                   const State& new_state) override;

  // Calls the corresponding callback to choose the surface.
  void ProvideOverlay(std::unique_ptr<AndroidOverlay> overlay);
  void ProvideTextureOwner();

  UseOverlayCB use_overlay_cb_;
  UseTextureOwnerCB use_texture_owner_cb_;
  AndroidOverlayFactoryCB factory_;
  State current_state_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MOCK_ANDROID_VIDEO_SURFACE_CHOOSER_H_
