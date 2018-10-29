// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_FRAME_MAPPER_FACTORY_H_
#define MEDIA_GPU_TEST_VIDEO_FRAME_MAPPER_FACTORY_H_

#include <memory>

#include "media/gpu/test/video_frame_mapper.h"

namespace media {
namespace test {

// A factory function for VideoFrameMapper.
// The appropriate VideoFrameMapper is a platform-dependent.
class VideoFrameMapperFactory {
 public:
  // Create an appropriate mapper on a platform.
  static std::unique_ptr<VideoFrameMapper> CreateMapper();
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_GENERIC_VIDEO_FRAME_MAPPER_FACTORY_H_
