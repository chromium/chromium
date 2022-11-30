// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_TESTING_CONTROLS_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_TESTING_CONTROLS_IMPL_H_

#include "services/video_capture/public/mojom/testing_controls.mojom.h"

namespace video_capture {

class TestingControlsImpl : public mojom::TestingControls {
 public:
  TestingControlsImpl();

  TestingControlsImpl(const TestingControlsImpl&) = delete;
  TestingControlsImpl& operator=(const TestingControlsImpl&) = delete;

  ~TestingControlsImpl() override;

  // mojom::TestingControls implementation.
  void Crash() override;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_TESTING_CONTROLS_IMPL_H_
