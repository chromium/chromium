// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_DEVICE_H_
#define MEDIA_GPU_V4L2_STATELESS_DEVICE_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/memory/ref_counted.h"
#include "media/base/video_codecs.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Encapsulates the v4l2 subsystem and prevents <linux/videodev2.h> from
// being included elsewhere with the possible exception of the codec specific
// delegates. This keeps all of the v4l2 driver specific structures in one
// place.
class MEDIA_GPU_EXPORT Device : public base::RefCountedThreadSafe<Device> {
 public:
  Device();
  virtual bool Open() = 0;
  void Close();

  // Walks through the list of formats returned by the VIDIOC_ENUM_FMT ioctl.
  // These are all of the compressed formats that the driver will accept.
  std::set<VideoCodec> EnumerateInputFormats();

  // VIDIOC_ENUM_FRAMESIZES
  std::pair<gfx::Size, gfx::Size> GetFrameResolutionRange(VideoCodec codec);

  // Uses the VIDIOC_QUERYCTRL and VIDIOC_QUERYMENU ioctls to list the
  // profiles of the input formats.
  std::vector<VideoCodecProfile> ProfilesForVideoCodec(VideoCodec codec);

  // Capabilities are queried using VIDIOC_QUERYCAP.  Stateless and
  // stateful drivers need different capabilities.
  virtual bool CheckCapabilities(VideoCodec codec) = 0;

 private:
  friend class base::RefCountedThreadSafe<Device>;

  // Stateless and stateful drivers have different fourcc values for
  // the same codec to designate stateful vs stateless.
  virtual uint32_t VideoCodecToV4L2PixFmt(VideoCodec codec) = 0;
  virtual std::string DevicePath() = 0;

  // The actual device fd.
  base::ScopedFD device_fd_;

 protected:
  virtual ~Device();
  int IoctlDevice(int request, void* arg);
  bool OpenDevice();
};

}  //  namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_DEVICE_H_
