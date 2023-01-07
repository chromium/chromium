// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp9_reference_frame_vector.h"

#include <bitset>

#include "media/gpu/vp9_picture.h"

namespace media {

Vp9ReferenceFrameVector::Vp9ReferenceFrameVector() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Vp9ReferenceFrameVector::~Vp9ReferenceFrameVector() = default;

// Refreshes |reference_frames_| slots with the current |pic|s frame header.
void Vp9ReferenceFrameVector::Refresh(scoped_refptr<VP9Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pic);
  const std::bitset<kVp9NumRefFrames> refresh_frame_flags(
      pic->frame_hdr->refresh_frame_flags);

  for (size_t i = 0; i < kVp9NumRefFrames; ++i) {
    if (refresh_frame_flags[i])
      reference_frames_[i] = pic;
  }
}

void Vp9ReferenceFrameVector::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reference_frames_.fill(nullptr);
}

scoped_refptr<VP9Picture> Vp9ReferenceFrameVector::GetFrame(
    size_t index) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return reference_frames_[index];
}

}  // namespace media
