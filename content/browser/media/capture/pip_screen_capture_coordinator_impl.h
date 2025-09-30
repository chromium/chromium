// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_IMPL_H_

#include <cstdint>

#include "base/observer_list.h"
#include "content/browser/media/capture/screen_capture_kit_device_utils_mac.h"

namespace content {

class WebContents;

class CONTENT_EXPORT PipScreenCaptureCoordinatorImpl {
 public:
  using NativeWindowId = NativeWindowIdMac;

  class Observer : public base::CheckedObserver {
   public:
    // Called with the NativeWindowId of the PiP window when it is
    // shown, or nullopt when it is closed.
    virtual void OnPipWindowIdChanged(
        std::optional<NativeWindowId> new_pip_window_id) = 0;
  };

  explicit PipScreenCaptureCoordinatorImpl();
  virtual ~PipScreenCaptureCoordinatorImpl();

  PipScreenCaptureCoordinatorImpl(const PipScreenCaptureCoordinatorImpl&) =
      delete;
  PipScreenCaptureCoordinatorImpl& operator=(
      const PipScreenCaptureCoordinatorImpl&) = delete;

  void OnPipShown(WebContents& pip_web_contents);
  void OnPipShown(NativeWindowId pip_window_id);
  void OnPipClosed();

  std::optional<NativeWindowId> PipWindowId() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  std::optional<NativeWindowId> pip_window_id_;
  base::ObserverList<Observer> observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_IMPL_H_
