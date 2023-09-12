// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_STATELESS_DEVICE_H_
#define MEDIA_GPU_V4L2_STATELESS_STATELESS_DEVICE_H_

#include "media/gpu/v4l2/stateless/device.h"

namespace media {
class MEDIA_GPU_EXPORT StatelessDevice : public Device {
 public:
  bool CheckCapabilities(VideoCodec codec) override;
  bool Open() override;

  bool IsCompressedVP9HeaderSupported();

 protected:
  ~StatelessDevice() override;

 private:
  uint32_t VideoCodecToV4L2PixFmt(VideoCodec codec) override;

  std::string DevicePath() override;
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_STATELESS_DEVICE_H_
