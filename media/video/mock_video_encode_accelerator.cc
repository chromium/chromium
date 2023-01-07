// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/mock_video_encode_accelerator.h"

#include "media/base/media_log.h"
#include "media/base/video_frame.h"

namespace media {

using ::testing::Invoke;

MockVideoEncodeAccelerator::MockVideoEncodeAccelerator() {
  // Delete |this| when Destroy() is called.
  ON_CALL(*this, Destroy())
      .WillByDefault(Invoke(this, &MockVideoEncodeAccelerator::DeleteThis));
}

MockVideoEncodeAccelerator::~MockVideoEncodeAccelerator() = default;

void MockVideoEncodeAccelerator::DeleteThis() {
  delete this;
}

}  // namespace media
