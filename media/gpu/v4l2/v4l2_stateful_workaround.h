// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_STATEFUL_WORKAROUND_H_
#define MEDIA_GPU_V4L2_V4L2_STATEFUL_WORKAROUND_H_

#include <memory>
#include <vector>

#include "media/base/video_types.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

class V4L2StatefulWorkaround {
 public:
  enum class Result {
    Success,      // The workaround is successfully applied.
    NotifyError,  // The caller must notify an error for Chrome. For example,
                  // VDA will call NotifyError() if this is returned.
  };

  virtual ~V4L2StatefulWorkaround() = default;

  // Apply the workaround.
  virtual Result Apply(const uint8_t* data, size_t size, size_t* endpos) = 0;

 protected:
  V4L2StatefulWorkaround() = default;

  DISALLOW_COPY_AND_ASSIGN(V4L2StatefulWorkaround);
};

// Create necessary workarounds on the device for |device_type| and |profile|.
std::vector<std::unique_ptr<V4L2StatefulWorkaround>>
CreateV4L2StatefulWorkarounds(V4L2Device::Type device_type,
                              VideoCodecProfile profile);

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_STATEFUL_WORKAROUND_H_
