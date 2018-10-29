// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/test/test_media_controller.h"

namespace media_session {
namespace test {

TestMediaController::TestMediaController() = default;

TestMediaController::~TestMediaController() = default;

mojom::MediaControllerPtr TestMediaController::CreateMediaControllerPtr() {
  mojom::MediaControllerPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void TestMediaController::ToggleSuspendResume() {
  ++toggle_suspend_resume_count_;
}

void TestMediaController::PreviousTrack() {
  ++previous_track_count_;
}

void TestMediaController::NextTrack() {
  ++next_track_count_;
}

}  // namespace test
}  // namespace media_session
