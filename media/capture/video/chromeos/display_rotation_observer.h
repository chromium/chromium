// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_DISPLAY_ROTATION_OBSERVER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_DISPLAY_ROTATION_OBSERVER_H_

#include "base/functional/callback.h"
#include "media/capture/video/chromeos/mojom/system_event_monitor.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// ScreenObserverDelegate is used to observe screen rotation on
// SystemEventMonitor. Its construction and destruction have to be on the ui
// thread. If not, it will hit a CHECK failure because it breaks mojo's thread
// safety.
class ScreenObserverDelegate : public cros::mojom::CrosDisplayObserver {
 public:
  using OnScreenRotationChangedCallback = base::RepeatingCallback<void(int)>;

  // When rotation of the screen is changed,
  // |on_screen_rotation_changed_callback| will be invoked on the ui thread.
  explicit ScreenObserverDelegate(
      OnScreenRotationChangedCallback on_screen_rotation_changed_callback);

  ~ScreenObserverDelegate() override;

  ScreenObserverDelegate() = delete;
  ScreenObserverDelegate(const ScreenObserverDelegate&) = delete;
  ScreenObserverDelegate& operator=(const ScreenObserverDelegate&) = delete;

  void OnDisplayRotationChanged(
      cros::mojom::ClockwiseRotation rotation) override;

 private:
  OnScreenRotationChangedCallback on_screen_rotation_changed_callback_;

  mojo::Receiver<cros::mojom::CrosDisplayObserver> receiver_{this};

  mojo::Remote<cros::mojom::CrosSystemEventMonitor> monitor_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_DISPLAY_ROTATION_OBSERVER_H_
