// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_TEST_MEDIA_CONTROLLER_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_TEST_MEDIA_CONTROLLER_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace media_session {
namespace test {

// Implements the MediaController mojo interface for tests.
class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP) TestMediaController
    : public mojom::MediaController {
 public:
  TestMediaController();
  ~TestMediaController() override;

  mojom::MediaControllerPtr CreateMediaControllerPtr();

  // mojom::MediaController:
  void Suspend() override {}
  void Resume() override {}
  void ToggleSuspendResume() override;
  void AddObserver(mojom::MediaSessionObserverPtr) override {}
  void PreviousTrack() override;
  void NextTrack() override;

  int toggle_suspend_resume_count() const {
    return toggle_suspend_resume_count_;
  }

  int previous_track_count() const { return previous_track_count_; }
  int next_track_count() const { return next_track_count_; }

 private:
  int toggle_suspend_resume_count_ = 0;
  int previous_track_count_ = 0;
  int next_track_count_ = 0;

  mojo::Binding<mojom::MediaController> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(TestMediaController);
};

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_TEST_MEDIA_CONTROLLER_H_
