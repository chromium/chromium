// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VP8_REFERENCE_FRAME_VECTOR_H_
#define MEDIA_GPU_VP8_REFERENCE_FRAME_VECTOR_H_

#include <array>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/parsers/vp8_parser.h"

namespace media {

class VP8Picture;

class Vp8ReferenceFrameVector {
 public:
  Vp8ReferenceFrameVector();

  Vp8ReferenceFrameVector(const Vp8ReferenceFrameVector&) = delete;
  Vp8ReferenceFrameVector& operator=(const Vp8ReferenceFrameVector&) = delete;

  ~Vp8ReferenceFrameVector();

  void Refresh(scoped_refptr<VP8Picture> pic);
  void Clear();

  scoped_refptr<VP8Picture> GetFrame(Vp8RefType type) const;

 private:
  std::array<scoped_refptr<VP8Picture>, kNumVp8ReferenceBuffers>
      reference_frames_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_VP8_REFERENCE_FRAME_VECTOR_H_
