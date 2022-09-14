// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vp8_reference_frame_vector.h"

#include "media/gpu/vp8_picture.h"

namespace media {

Vp8ReferenceFrameVector::Vp8ReferenceFrameVector() {
  // TODO(posciak): Remove this once VP8Decoder is created on the same thread
  // as its methods are called on.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Vp8ReferenceFrameVector::~Vp8ReferenceFrameVector() {
  // TODO(posciak): Add this once VP8Decoder is created on the same thread
  // as its methods are called on.
  // DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// Based on update_reference_frames() in libvpx: vp8/encoder/onyx_if.c
void Vp8ReferenceFrameVector::Refresh(scoped_refptr<VP8Picture> pic) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pic);

  bool keyframe = pic->frame_hdr->IsKeyframe();
  const auto& frame_hdr = pic->frame_hdr;

  if (keyframe) {
    reference_frames_[Vp8RefType::VP8_FRAME_LAST] = pic;
    reference_frames_[Vp8RefType::VP8_FRAME_GOLDEN] = pic;
    reference_frames_[Vp8RefType::VP8_FRAME_ALTREF] = pic;
    return;
  }

  if (frame_hdr->refresh_alternate_frame) {
    reference_frames_[Vp8RefType::VP8_FRAME_ALTREF] = pic;
  } else {
    switch (frame_hdr->copy_buffer_to_alternate) {
      case Vp8FrameHeader::COPY_LAST_TO_ALT:
        DCHECK(reference_frames_[Vp8RefType::VP8_FRAME_LAST]);
        reference_frames_[Vp8RefType::VP8_FRAME_ALTREF] =
            reference_frames_[Vp8RefType::VP8_FRAME_LAST];
        break;

      case Vp8FrameHeader::COPY_GOLDEN_TO_ALT:
        DCHECK(reference_frames_[Vp8RefType::VP8_FRAME_GOLDEN]);
        reference_frames_[Vp8RefType::VP8_FRAME_ALTREF] =
            reference_frames_[Vp8RefType::VP8_FRAME_GOLDEN];
        break;

      case Vp8FrameHeader::NO_ALT_REFRESH:
        DCHECK(reference_frames_[Vp8RefType::VP8_FRAME_ALTREF]);
        break;
    }
  }

  if (frame_hdr->refresh_golden_frame) {
    reference_frames_[Vp8RefType::VP8_FRAME_GOLDEN] = pic;
  } else {
    switch (frame_hdr->copy_buffer_to_golden) {
      case Vp8FrameHeader::COPY_LAST_TO_GOLDEN:
        DCHECK(reference_frames_[Vp8RefType::VP8_FRAME_LAST]);
        reference_frames_[Vp8RefType::VP8_FRAME_GOLDEN] =
            reference_frames_[Vp8RefType::VP8_FRAME_LAST];
        break;

      case Vp8FrameHeader::COPY_ALT_TO_GOLDEN:
        DCHECK(reference_frames_[Vp8RefType::VP8_FRAME_ALTREF]);
        reference_frames_[Vp8RefType::VP8_FRAME_GOLDEN] =
            reference_frames_[Vp8RefType::VP8_FRAME_ALTREF];
        break;

      case Vp8FrameHeader::NO_GOLDEN_REFRESH:
        DCHECK(reference_frames_[Vp8RefType::VP8_FRAME_GOLDEN]);
        break;
    }
  }

  if (frame_hdr->refresh_last)
    reference_frames_[Vp8RefType::VP8_FRAME_LAST] = pic;
  else
    DCHECK(reference_frames_[Vp8RefType::VP8_FRAME_LAST]);
}

void Vp8ReferenceFrameVector::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& f : reference_frames_)
    f = nullptr;
}

scoped_refptr<VP8Picture> Vp8ReferenceFrameVector::GetFrame(
    Vp8RefType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return reference_frames_[type];
}

}  // namespace media
