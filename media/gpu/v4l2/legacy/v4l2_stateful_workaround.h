// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_LEGACY_V4L2_STATEFUL_WORKAROUND_H_
#define MEDIA_GPU_V4L2_LEGACY_V4L2_STATEFUL_WORKAROUND_H_

#include <memory>
#include <string>
#include <vector>

#include "media/base/video_types.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

// TODO(hiroh): Remove this class once V4L2VideoDecodeAccelerator is removed.
class V4L2StatefulWorkaround {
 public:
  enum class Result {
    Success,      // The workaround is successfully applied.
    NotifyError,  // The caller must notify an error for Chrome. For example,
                  // VDA will call NotifyError() if this is returned.
  };

  V4L2StatefulWorkaround(const V4L2StatefulWorkaround&) = delete;
  V4L2StatefulWorkaround& operator=(const V4L2StatefulWorkaround&) = delete;

  virtual ~V4L2StatefulWorkaround() = default;

  // Apply the workaround.
  virtual Result Apply(const uint8_t* data, size_t size) = 0;

 protected:
  V4L2StatefulWorkaround() = default;
};

// Create necessary workarounds on the device for |device_type| and |profile|.
std::vector<std::unique_ptr<V4L2StatefulWorkaround>>
CreateV4L2StatefulWorkarounds(V4L2Device::Type device_type,
                              VideoCodecProfile profile);

// DecoderBuffer contains superframe in VP9 k-SVC stream but doesn't have
// superframe_index. This constructs superframe_index from side_data of
// DecoderBuffer which stands for sizes of frames in a superframe.
// |buffer| is replaced with a new DecoderBuffer, where superframe index is
// appended to |buffer| data. Besides, show_frame in the new DecoderBuffer is
// overwritten so that show_frame is one only in the top spatial layer.
// See go/VP9-k-SVC-Decoing-VAAPI for detail.
bool AppendVP9SuperFrameIndex(scoped_refptr<DecoderBuffer>& buffer);
}  // namespace media

#endif  // MEDIA_GPU_V4L2_LEGACY_V4L2_STATEFUL_WORKAROUND_H_
