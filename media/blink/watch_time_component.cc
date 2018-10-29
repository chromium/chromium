// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/watch_time_component.h"

#include "media/blink/media_blink_export.h"
#include "third_party/blink/public/platform/web_media_player.h"

namespace media {

template <typename T>
WatchTimeComponent<T>::WatchTimeComponent(
    T initial_value,
    std::vector<WatchTimeKey> keys_to_finalize,
    ValueToKeyCB value_to_key_cb,
    GetMediaTimeCB get_media_time_cb,
    mojom::WatchTimeRecorder* recorder)
    : keys_to_finalize_(std::move(keys_to_finalize)),
      value_to_key_cb_(std::move(value_to_key_cb)),
      get_media_time_cb_(std::move(get_media_time_cb)),
      recorder_(recorder),
      current_value_(initial_value),
      pending_value_(initial_value) {}

template <typename T>
WatchTimeComponent<T>::~WatchTimeComponent() = default;

template <typename T>
void WatchTimeComponent<T>::OnReportingStarted(
    base::TimeDelta start_timestamp) {
  start_timestamp_ = start_timestamp;
  end_timestamp_ = last_timestamp_ = kNoTimestamp;
}

template <typename T>
void WatchTimeComponent<T>::SetPendingValue(T new_value) {
  pending_value_ = new_value;
  if (current_value_ != new_value) {
    // Don't trample an existing finalize; the first takes precedence.
    //
    // Note: For components with trinary or higher state, which experience
    // multiple state changes during an existing finalize, this will drop all
    // watch time between the current and final state. E.g., state=0 {0ms} ->
    // state=1 {1ms} -> state=2 {2ms} will result in loss of state=1 watch time.
    if (end_timestamp_ != kNoTimestamp)
      return;

    end_timestamp_ = get_media_time_cb_.Run();
    return;
  }

  // Clear any pending finalize since we returned to the previous value before
  // the finalize could completed. I.e., assume this is a continuation.
  end_timestamp_ = kNoTimestamp;
}

template <typename T>
void WatchTimeComponent<T>::SetCurrentValue(T new_value) {
  current_value_ = new_value;
}

template <typename T>
void WatchTimeComponent<T>::RecordWatchTime(base::TimeDelta current_timestamp) {
  DCHECK_NE(current_timestamp, kNoTimestamp);
  DCHECK_NE(current_timestamp, kInfiniteDuration);
  DCHECK_GE(current_timestamp, base::TimeDelta());

  // If we're finalizing, use the media time at time of finalization. We only
  // use the |end_timestamp_| if it's less than the current timestamp, otherwise
  // we may report more watch time than expected.
  if (NeedsFinalize() && end_timestamp_ < current_timestamp)
    current_timestamp = end_timestamp_;

  // Don't update watch time if media time hasn't changed since the last run;
  // this may occur if a seek is taking some time to complete or the playback
  // is stalled for some reason.
  if (last_timestamp_ == current_timestamp)
    return;

  last_timestamp_ = current_timestamp;
  const base::TimeDelta elapsed = last_timestamp_ - start_timestamp_;
  if (elapsed <= base::TimeDelta())
    return;

  // If no value to key callback has been provided, record |elapsed| to every
  // key in the |keys_to_finalize_| list.
  if (!value_to_key_cb_) {
    for (auto k : keys_to_finalize_)
      recorder_->RecordWatchTime(k, elapsed);
    return;
  }

  // A conversion callback has been specified, so only report elapsed to the
  // key provided by the callback.
  //
  // Record watch time using |current_value_| and not |pending_value_| since
  // that transition should not happen until Finalize().
  recorder_->RecordWatchTime(value_to_key_cb_.Run(current_value_), elapsed);
}

template <typename T>
void WatchTimeComponent<T>::Finalize(
    std::vector<WatchTimeKey>* keys_to_finalize) {
  DCHECK(NeedsFinalize());
  // Update |current_value_| and |start_timestamp_| to |end_timestamp_| since
  // that's when the |pending_value_| was set.
  current_value_ = pending_value_;
  start_timestamp_ = end_timestamp_;

  // Complete the finalize and indicate which keys need to be finalized.
  end_timestamp_ = kNoTimestamp;
  keys_to_finalize->insert(keys_to_finalize->end(), keys_to_finalize_.begin(),
                           keys_to_finalize_.end());
  DCHECK(!NeedsFinalize());
}

template <typename T>
bool WatchTimeComponent<T>::NeedsFinalize() const {
  return end_timestamp_ != kNoTimestamp;
}

// Required to avoid linking errors since we've split this file into a .cc + .h
// file set instead of putting the function definitions in the header file. Any
// new component type must be added here.
//
// Note: These must be the last line in this file, otherwise you will also see
// linking errors since the templates won't have been fully defined prior.
template class MEDIA_BLINK_EXPORT WatchTimeComponent<bool>;
template class MEDIA_BLINK_EXPORT
    WatchTimeComponent<blink::WebMediaPlayer::DisplayType>;

}  // namespace media
