// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer.h"

#include <algorithm>
#include <memory>
#include <variant>

#include "base/logging.h"
#include "base/time/time.h"
namespace media {

Mp4Muxer::Mp4Muxer(AudioCodec audio_codec,
                   bool has_video,
                   bool has_audio,
                   std::unique_ptr<Mp4MuxerDelegateInterface> delegate,
                   std::optional<base::TimeDelta> max_data_output_interval)
    : mp4_muxer_delegate_(std::move(delegate)),
      max_data_output_interval_(
          std::max(max_data_output_interval.value_or(base::TimeDelta()),
                   kMinimumForcedOutputDuration)),
      has_video_(has_video),
      has_audio_(has_audio),
      audio_codec_(audio_codec) {
  CHECK(has_video_ || has_audio_);
  CHECK(!has_audio || audio_codec_ == AudioCodec::kAAC ||
        audio_codec_ == AudioCodec::kOpus);

  DVLOG(1) << __func__ << ", Max output interval in seconds: "
           << max_data_output_interval_.InSeconds();

  // Creation can be done on a different sequence than main activities.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

Mp4Muxer::~Mp4Muxer() = default;

bool Mp4Muxer::PutFrame(EncodedFrame frame,
                        base::TimeDelta relative_timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioParameters* audio_params = std::get_if<AudioParameters>(&frame.params);
  if (audio_params) {
    CHECK(has_audio_);
    // The first audio sample should have code description.
    mp4_muxer_delegate_->AddAudioFrame(*audio_params, std::move(frame.data),
                                       frame.codec_description,
                                       base::TimeTicks() + relative_timestamp);
    seen_audio_ = true;
    if (!has_video_ || video_ended_) {
      // If there is no video, we can try flush the fragment regardless of
      // video key frame.
      MaybeForceFragmentFlush(relative_timestamp);
    }
  } else {
    auto* video_params = std::get_if<VideoParameters>(&frame.params);
    CHECK(video_params);
    CHECK(has_video_);

    // The `trun` box, which holds information for each sample such as duration
    // and size, cannot be split because the `count` property needs to have the
    // exact number of sample entries. The unit of the `trun` box is per
    // fragment, which is based on the video key frame. So, it checks flush
    // only when the next key frame arrives.
    if (frame.data->is_key_frame()) {
      MaybeForceFragmentFlush(relative_timestamp);
    }
    mp4_muxer_delegate_->AddVideoFrame(*video_params, std::move(frame.data),
                                       frame.codec_description,
                                       base::TimeTicks() + relative_timestamp);
  }
  return true;
}

void Mp4Muxer::MaybeForceFragmentFlush(
    base::TimeDelta media_relative_timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Forces a fragment flush if the time since the last flush exceeds the max
  // data output interval. This ensures that data is output periodically.
  if (flush_origin_.has_value()) {
    if (media_relative_timestamp - *flush_origin_ >=
        max_data_output_interval_) {
      mp4_muxer_delegate_->FlushFragment();
      flush_origin_ = media_relative_timestamp;
    }
  } else {
    flush_origin_ = media_relative_timestamp;
  }
}

bool Mp4Muxer::Flush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << __func__ << ", Flush called ";

  if (!mp4_muxer_delegate_->Flush()) {
    DVLOG(1) << __func__ << ", Flush failed ";
    return false;
  }

  return true;
}

void Mp4Muxer::OnVideoEnded() {
  video_ended_ = true;
}

}  // namespace media
