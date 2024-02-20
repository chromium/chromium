// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
#define CONTENT_PUBLIC_TEST_MOCK_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_

#include "content/browser/picture_in_picture/video_picture_in_picture_window_controller_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockVideoPictureInPictureWindowControllerImpl
    : public content::VideoPictureInPictureWindowControllerImpl {
 public:
  explicit MockVideoPictureInPictureWindowControllerImpl(
      content::WebContents* web_contents);

  ~MockVideoPictureInPictureWindowControllerImpl() override;

  MockVideoPictureInPictureWindowControllerImpl(
      const MockVideoPictureInPictureWindowControllerImpl&) = delete;
  MockVideoPictureInPictureWindowControllerImpl& operator=(
      const MockVideoPictureInPictureWindowControllerImpl&) = delete;

  MOCK_METHOD1(SetOnWindowCreatedNotifyObserversCallback,
               void(base::OnceClosure));
};

}  //  namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_VIDEO_PICTURE_IN_PICTURE_WINDOW_CONTROLLER_IMPL_H_
