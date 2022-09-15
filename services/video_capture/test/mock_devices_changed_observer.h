// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_TEST_MOCK_DEVICES_CHANGED_OBSERVER_H_
#define SERVICES_VIDEO_CAPTURE_TEST_MOCK_DEVICES_CHANGED_OBSERVER_H_

#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockDevicesChangedObserver : public mojom::DevicesChangedObserver {
 public:
  MockDevicesChangedObserver();
  ~MockDevicesChangedObserver() override;

  // mojom::DevicesChangedObserver
  MOCK_METHOD0(OnDevicesChanged, void());
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_TEST_MOCK_DEVICES_CHANGED_OBSERVER_H_
