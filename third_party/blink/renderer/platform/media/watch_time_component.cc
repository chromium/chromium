// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/watch_time_component.h"

#include "base/time/time.h"
#include "media/mojo/mojom/watch_time_recorder.mojom-blink.h"
#include "third_party/blink/public/platform/web_media_player.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

template <typename T>
WatchTimeComponent<T>::WatchTimeComponent(
    T initial_value,
    Vector<media::WatchTimeKey> keys_to_finalize,
    ValueToKeyCB value_to_key_cb,
    GetMediaTimeCB get_media_time_cb,
    media::mojom::blink::WatchTimeRecorder* recorder)
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
  end_timestamp_ = last_timestamp_ = media::kNoTimestamp;
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
    if (end_timestamp_ != media::kNoTimestamp)
      return;

    end_timestamp_ = get_media_time_cb_.Run();
    return;
  }

  // Clear any pending finalize since we returned to the previous value before
  // the finalize could completed. I.e., assume this is a continuation.
  end_timestamp_ = media::kNoTimestamp;
}

template <typename T>
void WatchTimeComponent<T>::SetCurrentValue(T new_value) {
  current_value_ = new_value;
}

template <typename T>
void WatchTimeComponent<T>::RecordWatchTime(base::TimeDelta current_timestamp) {
  DCHECK_NE(current_timestamp, media::kNoTimestamp);
  DCHECK_NE(current_timestamp, media::kInfiniteDuration);
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
  // keys provided by the callback.
  //
  // Record watch time using |current_value_| and not |pending_value_| since
  // that transition should not happen until Finalize().
  for (auto key : value_to_key_cb_.Run(current_value_)) {
    recorder_->RecordWatchTime(key, elapsed);
  }
}

template <typename T>
void WatchTimeComponent<T>::Finalize(
    Vector<media::WatchTimeKey>* keys_to_finalize) {
  DCHECK(NeedsFinalize());
  // Update |current_value_| and |start_timestamp_| to |end_timestamp_| since
  // that's when the |pending_value_| was set.
  current_value_ = pending_value_;
  start_timestamp_ = end_timestamp_;

  // Complete the finalize and indicate which keys need to be finalized.
  end_timestamp_ = media::kNoTimestamp;
  keys_to_finalize->AppendVector(keys_to_finalize_);
  DCHECK(!NeedsFinalize());
}

template <typename T>
bool WatchTimeComponent<T>::NeedsFinalize() const {
  return end_timestamp_ != media::kNoTimestamp;
}

// Required to avoid linking errors since we've split this file into a .cc + .h
// file set instead of putting the function definitions in the header file. Any
// new component type must be added here.
//
// Note: These must be the last line in this file, otherwise you will also see
// linking errors since the templates won't have been fully defined prior.
template class PLATFORM_EXPORT WatchTimeComponent<bool>;
template class PLATFORM_EXPORT WatchTimeComponent<WebMediaPlayer::DisplayType>;

}  // namespace blink
