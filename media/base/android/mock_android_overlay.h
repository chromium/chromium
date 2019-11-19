// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MOCK_ANDROID_OVERLAY_H_
#define MEDIA_BASE_ANDROID_MOCK_ANDROID_OVERLAY_H_

#include "media/base/android/android_overlay.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/android/test_destruction_observable.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// AndroidOverlay implementation that supports weak ptrs.
class MockAndroidOverlay : public testing::NiceMock<AndroidOverlay>,
                           public DestructionObservable {
 public:
  MockAndroidOverlay();
  ~MockAndroidOverlay() override;

  MOCK_METHOD1(ScheduleLayout, void(const gfx::Rect&));
  MOCK_CONST_METHOD0(GetJavaSurface, base::android::JavaRef<jobject>&());

  // Set |config_|.  Sometimes, it's convenient to do this after construction,
  // especially if one must create the overlay before the factory provides it
  // via CreateOverlay.  That's helpful to set test expectations.
  void SetConfig(AndroidOverlayConfig config);

  // Return the config, if any, so that tests can check it.
  AndroidOverlayConfig* config() const { return config_.get(); }

  // Set of callbacks that we provide to control the overlay once you've handed
  // off ownership of it.  Will return false if the overlay has been destroyed.
  using ControlCallback = base::RepeatingCallback<void()>;
  struct Callbacks {
    Callbacks();
    Callbacks(const Callbacks&);
    ~Callbacks();

    ControlCallback OverlayReady;
    ControlCallback OverlayFailed;
    ControlCallback SurfaceDestroyed;
    base::RepeatingCallback<void(bool)> PowerEfficientState;
  };

  // Return callbacks that can be used to control the overlay.
  Callbacks GetCallbacks();

  MOCK_METHOD0(MockAddSurfaceDestroyedCallback, void());
  void AddSurfaceDestroyedCallback(
      AndroidOverlayConfig::DestroyedCB cb) override;

  // Send callbacks.
  void OnOverlayReady();
  void OnOverlayFailed();
  void OnSurfaceDestroyed();
  void OnPowerEfficientState(bool state);

 private:
  // Initial configuration, mostly for callbacks.
  std::unique_ptr<AndroidOverlayConfig> config_;

  base::WeakPtrFactory<MockAndroidOverlay> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockAndroidOverlay);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MOCK_ANDROID_OVERLAY_H_
