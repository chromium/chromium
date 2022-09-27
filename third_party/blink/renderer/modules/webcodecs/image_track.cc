// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_track.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/image-decoders/image_animation.h"

namespace blink {

ImageTrack::ImageTrack(ImageTrackList* image_track_list,
                       wtf_size_t id,
                       uint32_t frame_count,
                       int repetition_count,
                       bool selected)
    : id_(id),
      image_track_list_(image_track_list),
      frame_count_(frame_count),
      repetition_count_(repetition_count),
      selected_(selected) {}

ImageTrack::~ImageTrack() = default;

uint32_t ImageTrack::frameCount() const {
  return frame_count_;
}

bool ImageTrack::animated() const {
  return frame_count_ > 1 || repetition_count_ != kAnimationNone;
}

float ImageTrack::repetitionCount() const {
  if (repetition_count_ == kAnimationNone ||
      repetition_count_ == kAnimationLoopOnce) {
    return 0;
  }

  if (repetition_count_ == kAnimationLoopInfinite)
    return INFINITY;

  return repetition_count_;
}

bool ImageTrack::selected() const {
  return selected_;
}

void ImageTrack::setSelected(bool selected) {
  if (selected == selected_)
    return;

  selected_ = selected;

  // If the track has been disconnected, a JS ref on the object may still exist
  // and trigger calls here. We should do nothing in this case.
  if (image_track_list_)
    image_track_list_->OnTrackSelectionChanged(id_);
}

void ImageTrack::UpdateTrack(uint32_t frame_count, int repetition_count) {
  DCHECK(image_track_list_);

  const bool was_animated = animated();
  frame_count_ = frame_count;
  repetition_count_ = repetition_count;

  // Changes from still to animated are not allowed since they can cause sites
  // to think there are no further frames to decode in the streaming case.
  if (!was_animated)
    DCHECK_EQ(was_animated, animated());
}

void ImageTrack::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(image_track_list_);
}

}  // namespace blink
