// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_H_
#define MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_H_

#include "base/memory/ref_counted.h"
#include "media/base/video_frame.h"

namespace media {

class StatelessDecodeSurface : public base::RefCounted<StatelessDecodeSurface> {
 public:
  StatelessDecodeSurface(scoped_refptr<VideoFrame> frame);
  StatelessDecodeSurface();

  StatelessDecodeSurface(const StatelessDecodeSurface&) = delete;
  StatelessDecodeSurface& operator=(const StatelessDecodeSurface&) = delete;

 protected:
  virtual ~StatelessDecodeSurface();
  friend class base::RefCounted<StatelessDecodeSurface>;

 private:
  scoped_refptr<VideoFrame> video_frame_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_STATELESS_DECODE_SURFACE_H_
