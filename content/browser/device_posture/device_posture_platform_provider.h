// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_H_
#define CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_H_

#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

class WebContents;

// This the base class for platform-specific device posture provider
// implementations. In typical usage a single instance is owned by
// DeviceService.
class DevicePosturePlatformProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDevicePostureChanged(
        const blink::mojom::DevicePostureType& posture) {}
    virtual void OnViewportSegmentsChanged(
        const std::vector<gfx::Rect>& segments) {}
  };

  // Returns a DevicePostureProvider for the current platform.
  static std::unique_ptr<DevicePosturePlatformProvider> Create(
      WebContents* web_contents);

  virtual ~DevicePosturePlatformProvider();

  blink::mojom::DevicePostureType GetDevicePosture();
  const std::vector<gfx::Rect>& GetViewportSegments();

  DevicePosturePlatformProvider(const DevicePosturePlatformProvider&) = delete;
  DevicePosturePlatformProvider& operator=(
      const DevicePosturePlatformProvider&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  virtual void StartListening() = 0;
  virtual void StopListening() = 0;

  DevicePosturePlatformProvider();

  void NotifyDevicePostureChanged(
      const blink::mojom::DevicePostureType& posture);
  void NotifyWindowSegmentsChanged(const std::vector<gfx::Rect>& segments);

  blink::mojom::DevicePostureType current_posture_ =
      blink::mojom::DevicePostureType::kContinuous;
  std::vector<gfx::Rect> current_viewport_segments_;

 private:
  // DevicePosturePlatformProvider observers are expected to unregister
  // themselves.
  base::ObserverList<Observer> observers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_POSTURE_DEVICE_POSTURE_PLATFORM_PROVIDER_H_
