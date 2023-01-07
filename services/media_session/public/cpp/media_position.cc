// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/media_position.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"

namespace media_session {

MediaPosition::MediaPosition() = default;

MediaPosition::MediaPosition(double playback_rate,
                             base::TimeDelta duration,
                             base::TimeDelta position,
                             bool end_of_media)
    : playback_rate_(playback_rate),
      duration_(duration),
      position_(position),
      last_updated_time_(base::TimeTicks::Now()),
      end_of_media_(end_of_media) {
  DCHECK(duration_ >= base::Seconds(0));
  DCHECK(position_ >= base::Seconds(0));
  DCHECK(position_ <= duration_);
}

MediaPosition::~MediaPosition() = default;

base::TimeDelta MediaPosition::duration() const {
  return duration_;
}

double MediaPosition::playback_rate() const {
  return playback_rate_;
}

base::TimeTicks MediaPosition::last_updated_time() const {
  return last_updated_time_;
}

base::TimeDelta MediaPosition::GetPosition() const {
  return GetPositionAtTime(base::TimeTicks::Now());
}

base::TimeDelta MediaPosition::GetPositionAtTime(base::TimeTicks time) const {
  base::TimeDelta delta = time - last_updated_time_;

  // It's possible to get a time query that is *before* the last updated time
  // since |time| and |last_updated_time| generally come from different
  // processes. When we get a negative value, just assume effectively no time
  // has passed.
  if (delta.is_negative()) {
    DCHECK(!base::TimeTicks::IsConsistentAcrossProcesses());
    delta = base::Microseconds(0);
  }

  base::TimeDelta elapsed_time = playback_rate_ * delta;
  base::TimeDelta updated_position = position_ + elapsed_time;

  base::TimeDelta start = base::Seconds(0);

  if (updated_position <= start)
    return start;
  else if (updated_position >= duration_)
    return duration_;
  else
    return updated_position;
}

bool MediaPosition::operator==(const MediaPosition& other) const {
  if (playback_rate_ != other.playback_rate_ || duration_ != other.duration_ ||
      end_of_media_ != other.end_of_media_) {
    return false;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  return GetPositionAtTime(now) == other.GetPositionAtTime(now);
}

bool MediaPosition::operator!=(const MediaPosition& other) const {
  return !(*this == other);
}

std::string MediaPosition::ToString() const {
  return base::StringPrintf(
      "playback_rate=%f duration=%f current_time=%f end_of_media=%s",
      playback_rate_, duration_.InSecondsF(), position_.InSecondsF(),
      end_of_media_ ? "true" : "false");
}

}  // namespace media_session
