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

  // As part of the stateless uAPI the headers are parsed by the client
  // and sent to the device. When |request_fd| is invalid the driver parses
  // the header right away instead of waiting until the compressed data
  // is present. This is used when the format is being negotiated because
  // the driver will only expose formats that the bitstream can decode into.
  bool SetHeaders(void* ctrls, const base::ScopedFD& request_fd);

 protected:
  ~StatelessDevice() override;

 private:
  uint32_t VideoCodecToV4L2PixFmt(VideoCodec codec) override;

  std::string DevicePath() override;
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_STATELESS_DEVICE_H_
