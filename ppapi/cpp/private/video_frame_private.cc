// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/video_frame_private.h"

namespace pp {

VideoFrame_Private::VideoFrame_Private()
    : video_frame_() {
  video_frame_.image_data = image_data_.pp_resource();
}

VideoFrame_Private::VideoFrame_Private(const ImageData& image_data,
                                       PP_TimeTicks timestamp)
    : image_data_(image_data), video_frame_() {
  video_frame_.timestamp = timestamp;
  video_frame_.image_data = image_data_.pp_resource();
}

VideoFrame_Private::VideoFrame_Private(
    PassRef,
    const PP_VideoFrame_Private& pp_video_frame)
    : video_frame_(pp_video_frame) {
  // Take over the image_data resource in the frame.
  image_data_ = ImageData(PASS_REF, video_frame_.image_data);
}

VideoFrame_Private::VideoFrame_Private(const VideoFrame_Private& other)
    : video_frame_() {
  set_image_data(other.image_data());
  set_timestamp(other.timestamp());
}

VideoFrame_Private::~VideoFrame_Private() {
}

VideoFrame_Private& VideoFrame_Private::operator=(
    const VideoFrame_Private& other) {
  if (this == &other)
    return *this;

  set_image_data(other.image_data());
  set_timestamp(other.timestamp());

  return *this;
}

}  // namespace pp
