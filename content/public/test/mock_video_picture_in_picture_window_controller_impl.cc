// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_video_picture_in_picture_window_controller_impl.h"

namespace content {

MockVideoPictureInPictureWindowControllerImpl::
    MockVideoPictureInPictureWindowControllerImpl(
        content::WebContents* web_contents)
    : content::VideoPictureInPictureWindowControllerImpl(web_contents) {}

MockVideoPictureInPictureWindowControllerImpl::
    ~MockVideoPictureInPictureWindowControllerImpl() = default;

}  // namespace content
