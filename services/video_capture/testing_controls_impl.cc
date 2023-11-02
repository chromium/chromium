// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/testing_controls_impl.h"

#include "base/check.h"

namespace video_capture {

TestingControlsImpl::TestingControlsImpl() = default;

TestingControlsImpl::~TestingControlsImpl() = default;

void TestingControlsImpl::Crash() {
  CHECK(false) << "This is an intentional crash for the purpose of testing";
}

}  // namespace video_capture
