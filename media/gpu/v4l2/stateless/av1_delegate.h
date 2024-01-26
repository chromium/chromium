// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_STATELESS_AV1_DELEGATE_H_
#define MEDIA_GPU_V4L2_STATELESS_AV1_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/av1_decoder.h"

#ifndef V4L2_AV1_RESTORATION_TILESIZE_MAX
#define V4L2_AV1_RESTORATION_TILESIZE_MAX 256
#endif

namespace media {

class StatelessDecodeSurfaceHandler;

class AV1Delegate : public AV1Decoder::AV1Accelerator {
 public:
  explicit AV1Delegate(StatelessDecodeSurfaceHandler* surface_handler);

  AV1Delegate(const AV1Delegate&) = delete;
  AV1Delegate& operator=(const AV1Delegate&) = delete;

  ~AV1Delegate() override;

  // AV1Decoder::AV1Accelerator implementation.
  scoped_refptr<AV1Picture> CreateAV1Picture(bool apply_grain) override;
  AV1Delegate::Status SubmitDecode(
      const AV1Picture& pic,
      const libgav1::ObuSequenceHeader& sequence_header,
      const AV1ReferenceFrameVector& ref_frames,
      const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
      base::span<const uint8_t> stream) override;
  bool OutputPicture(const AV1Picture& pic) override;

 private:
  raw_ptr<StatelessDecodeSurfaceHandler> const surface_handler_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_STATELESS_AV1_DELEGATE_H_
