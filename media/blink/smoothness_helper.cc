// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/smoothness_helper.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "media/learning/common/learning_task_controller.h"

namespace {
static constexpr base::TimeDelta kSegmentSize =
    base::TimeDelta::FromSeconds(60);
}

namespace media {

using learning::FeatureVector;
using learning::LearningTaskController;
using learning::TargetValue;

class SmoothnessHelperImpl : public SmoothnessHelper {
 public:
  SmoothnessHelperImpl(std::unique_ptr<LearningTaskController> controller,
                       const FeatureVector& features,
                       Client* player)
      : controller_(std::move(controller)),
        features_(features),
        player_(player) {}

  // This will ignore the last segment, if any, which is fine since it's not
  // a complete segment.
  ~SmoothnessHelperImpl() override = default;

  void NotifyPlayState(bool playing) override {
    if (playing) {
      if (segment_decoded_frames_)
        return;

      // We're starting a new playback, so record the baseline frame counts.
      segment_dropped_frames_ = player_->DroppedFrameCount();
      segment_decoded_frames_ = player_->DecodedFrameCount();
      worst_segment_during_playback_ = TargetValue(0);

      DCHECK(!id_);

      // Don't bother to start the observation until the timer fires, since we
      // don't wanto to record short playbacks.

      update_timer_.Start(FROM_HERE, kSegmentSize,
                          base::BindRepeating(&SmoothnessHelperImpl::OnTimer,
                                              base::Unretained(this)));
    } else {
      if (!segment_decoded_frames_)
        return;

      // If we started an observation, then complete it.  Otherwise, the segment
      // wasn't long enough.  Note that we also don't update the worst NNR
      // rate here, so that we don't include very short partial segments that
      // might be artificially high.  Note that this might be a bad idea; if
      // the site detects bad playback and adapts before we've measured one
      // segment, then we'll never record those NNRs.  We might want to allow
      // the final segment to be smaller than |kSegmentSize|, as long as it's
      // not too small.
      if (id_)
        controller_->CompleteObservation(*id_, worst_segment_during_playback_);

      // End the segment and the playback.
      segment_decoded_frames_.reset();
      segment_dropped_frames_.reset();
      update_timer_.Stop();
      id_.reset();
    }
  }

  // Split playback into segments of length |kSegmentSize|, and update the
  // default value of the current playback.
  void OnTimer() {
    DCHECK(segment_decoded_frames_);

    auto new_dropped_frames = player_->DroppedFrameCount();
    auto dropped_frames = new_dropped_frames - *segment_dropped_frames_;
    segment_dropped_frames_ = new_dropped_frames;

    auto new_decoded_frames = player_->DecodedFrameCount();
    auto decoded_frames = new_decoded_frames - *segment_decoded_frames_;
    segment_decoded_frames_ = new_decoded_frames;

    if (!decoded_frames)
      return;

    // The target value is just the percentage of dropped frames.
    auto target = TargetValue(((double)dropped_frames) / decoded_frames);

    // See if this is worse than any previous segment.
    if (target > worst_segment_during_playback_)
      worst_segment_during_playback_ = target;

    // Start an observation for this playback, or update the default.
    if (!id_) {
      id_ = base::UnguessableToken::Create();
      controller_->BeginObservation(*id_, features_,
                                    worst_segment_during_playback_);
    } else {
      controller_->UpdateDefaultTarget(*id_, worst_segment_during_playback_);
    }
  }

  // Current dropped, decoded frames at the start of the segment, if any.
  base::Optional<int64_t> segment_decoded_frames_;
  base::Optional<int64_t> segment_dropped_frames_;

  // Of all the segments in this playback, this is the worst NNR ratio.
  TargetValue worst_segment_during_playback_;

  std::unique_ptr<LearningTaskController> controller_;

  FeatureVector features_;

  base::RepeatingTimer update_timer_;

  // WebMediaPlayer which will tell us about the decoded / dropped frame counts.
  Client* player_;

  // If an observation is in progress, then this is the id.
  base::Optional<base::UnguessableToken> id_;
};

// static
std::unique_ptr<SmoothnessHelper> SmoothnessHelper::Create(
    std::unique_ptr<LearningTaskController> controller,
    const FeatureVector& features,
    Client* player) {
  return std::make_unique<SmoothnessHelperImpl>(std::move(controller), features,
                                                player);
}

// static
base::TimeDelta SmoothnessHelper::SegmentSizeForTesting() {
  return kSegmentSize;
}

}  // namespace media
