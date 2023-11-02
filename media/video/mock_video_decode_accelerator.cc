// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/mock_video_decode_accelerator.h"

namespace media {

using ::testing::Invoke;

MockVideoDecodeAccelerator::MockVideoDecodeAccelerator() {
  // Delete |this| when Destroy() is called.
  ON_CALL(*this, Destroy())
      .WillByDefault(Invoke(this, &MockVideoDecodeAccelerator::DeleteThis));
}

MockVideoDecodeAccelerator::~MockVideoDecodeAccelerator() = default;

void MockVideoDecodeAccelerator::DeleteThis() { delete this; }

}  // namespace media
