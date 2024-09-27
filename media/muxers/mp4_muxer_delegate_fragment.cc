// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/muxers/mp4_muxer_delegate_fragment.h"

#include "base/notreached.h"
#include "media/base/decoder_buffer.h"
#include "media/muxers/mp4_muxer_context.h"

namespace media {

namespace {

using mp4::writable_boxes::FragmentSampleFlags;
using mp4::writable_boxes::TrackFragmentHeaderFlags;
using mp4::writable_boxes::TrackFragmentRunFlags;

}  // namespace

Mp4MuxerDelegateFragment::Mp4MuxerDelegateFragment(Mp4MuxerContext& context,
                                                   int video_track_id,
                                                   int audio_track_id,
                                                   uint32_t sequence_number)
    : context_(context), moof_(sequence_number) {
  // We preallocate space for two tracks and initialize the track id.
  moof_.track_fragments.emplace_back(mp4::writable_boxes::TrackFragment());
  moof_.track_fragments.emplace_back(mp4::writable_boxes::TrackFragment());

  // The `mdat` box is a container for media data and it will be
  // created for each track.
  mdat_.track_data.emplace_back(std::vector<uint8_t>());
  mdat_.track_data.emplace_back(std::vector<uint8_t>());

  AddNewTrack(kDefaultVideoIndex);
  AddNewTrack(kDefaultAudioIndex);

  // New track is added, so we can finally set track id.
  moof_.track_fragments[kDefaultVideoIndex].header.track_id = video_track_id;
  moof_.track_fragments[kDefaultVideoIndex].decode_time.track_id =
      video_track_id;
  moof_.track_fragments[kDefaultAudioIndex].header.track_id = audio_track_id;
  moof_.track_fragments[kDefaultAudioIndex].decode_time.track_id =
      audio_track_id;
}

bool Mp4MuxerDelegateFragment::HasSamples() const {
  // Ensure fragment is not empty.
  for (auto& track : moof_.track_fragments) {
    if (track.run.sample_count > 0) {
      return true;
    }
  }
  return false;
}

void Mp4MuxerDelegateFragment::AddVideoData(
    scoped_refptr<DecoderBuffer> encoded_data,
    base::TimeTicks timestamp) {
  // Add sample.
  mp4::writable_boxes::TrackFragmentRun& video_trun =
      moof_.track_fragments[kDefaultVideoIndex].run;
  AddDataToRun(video_trun, *encoded_data, timestamp);

  // Add sample data to the data box.
  AddDataToMdat(mdat_.track_data[kDefaultVideoIndex], *encoded_data);
}

void Mp4MuxerDelegateFragment::AddAudioData(
    scoped_refptr<DecoderBuffer> encoded_data,
    base::TimeTicks timestamp) {
  // Add sample.
  mp4::writable_boxes::TrackFragmentRun& audio_trun =
      moof_.track_fragments[kDefaultAudioIndex].run;

  AddDataToRun(audio_trun, *encoded_data, timestamp);
  // Add sample data to the data box.
  AddDataToMdat(mdat_.track_data[kDefaultAudioIndex], *encoded_data);
}

void Mp4MuxerDelegateFragment::AddVideoLastTimestamp(
    base::TimeDelta timestamp) {
  mp4::writable_boxes::TrackFragmentRun& video_trun =
      moof_.track_fragments[kDefaultVideoIndex].run;
  AddLastTimestamp(video_trun, timestamp);
}

void Mp4MuxerDelegateFragment::AddAudioLastTimestamp(
    base::TimeDelta timestamp) {
  mp4::writable_boxes::TrackFragmentRun& audio_trun =
      moof_.track_fragments[kDefaultAudioIndex].run;
  AddLastTimestamp(audio_trun, timestamp);
}

base::TimeTicks Mp4MuxerDelegateFragment::GetVideoStartTimestamp() const {
  if (moof_.track_fragments[kDefaultVideoIndex].run.sample_count == 0) {
    return base::TimeTicks();
  }

  return moof_.track_fragments[kDefaultVideoIndex].run.sample_timestamps[0];
}

const mp4::writable_boxes::MovieFragment&
Mp4MuxerDelegateFragment::GetMovieFragment() const {
  return moof_;
}

const mp4::writable_boxes::MediaData& Mp4MuxerDelegateFragment::GetMediaData()
    const {
  return mdat_;
}

size_t Mp4MuxerDelegateFragment::GetVideoSampleSize() const {
  return moof_.track_fragments[kDefaultVideoIndex].run.sample_count;
}

size_t Mp4MuxerDelegateFragment::GetAudioSampleSize() const {
  return moof_.track_fragments[kDefaultAudioIndex].run.sample_count;
}

void Mp4MuxerDelegateFragment::Finalize(base::TimeTicks start_audio_time,
                                        base::TimeTicks start_video_time) {
  // It corrects the order of the `trun` box by added track index of the 'mov'
  // box by using the context object that has correct track index info.

  SetBaseDecodeTime(start_audio_time, start_video_time);

  // It checks total valid fragments based on the context_ because the current
  // fragment may have different order from the first fragment that decides the
  // track index order. If it is different, it swaps the track index of the
  // of the container.
  bool swap = false;
  if (context_->GetAudioTrack().has_value()) {
    if (context_->GetAudioTrack().value().index != kDefaultAudioIndex) {
      // kDefaultAudioIndex is 0, which means the real audio track is 1, so
      // we need to swap the internal containers.
      swap = true;
    }
  } else {
    // Fragment is created during frame is added so it should have
    // at least one track.
    CHECK(context_->GetVideoTrack().has_value());
    swap = true;
  }

  if (swap) {
    std::swap(moof_.track_fragments[kDefaultAudioIndex],
              moof_.track_fragments[kDefaultVideoIndex]);
    std::swap(mdat_.track_data[kDefaultAudioIndex],
              mdat_.track_data[kDefaultVideoIndex]);
  }

  size_t valid_track_count = context_->GetVideoTrack().has_value() ? 1 : 0;
  valid_track_count += context_->GetAudioTrack().has_value() ? 1 : 0;

  if (valid_track_count == 2) {
    return;
  } else if (valid_track_count == 1) {
    // If there is only one track, we need to finalize the fragment.
    // We preallocate space for two tracks.
    moof_.track_fragments.erase(moof_.track_fragments.begin() + 1);
    mdat_.track_data.erase(mdat_.track_data.begin() + 1);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void Mp4MuxerDelegateFragment::AddNewTrack(uint32_t track_index) {
  bool audio = (track_index == kDefaultAudioIndex);

  mp4::writable_boxes::TrackFragment track_fragment;

  // `default-sample-flags`.
  std::vector<mp4::writable_boxes::FragmentSampleFlags> sample_flags;
  if (audio) {
    sample_flags.emplace_back(FragmentSampleFlags::kSampleFlagDependsNo);
  } else {
    sample_flags.emplace_back(FragmentSampleFlags::kSampleFlagIsNonSync);
    sample_flags.emplace_back(FragmentSampleFlags::kSampleFlagDependsYes);
  }

  track_fragment.header.default_sample_flags =
      BuildFlags<mp4::writable_boxes::FragmentSampleFlags>(sample_flags);

  track_fragment.header.default_sample_duration = base::TimeDelta();
  track_fragment.header.default_sample_size = 0;

  std::vector<mp4::writable_boxes::TrackFragmentHeaderFlags>
      fragment_header_flags = {
          TrackFragmentHeaderFlags::kDefaultBaseIsMoof,
          TrackFragmentHeaderFlags::kkDefaultSampleFlagsPresent
          // TODO(crbug.com/40275472).
          // TrackFragmentHeaderFlags::kDefaultSampleDurationPresent,
      };
  track_fragment.header.flags =
      BuildFlags<mp4::writable_boxes::TrackFragmentHeaderFlags>(
          fragment_header_flags);

  // `trun`.
  track_fragment.run = {};
  track_fragment.run.sample_count = 0;

  std::vector<mp4::writable_boxes::TrackFragmentRunFlags> fragment_run_flags = {
      TrackFragmentRunFlags::kDataOffsetPresent,
      TrackFragmentRunFlags::kSampleDurationPresent,
      TrackFragmentRunFlags::kSampleSizePresent};

  if (audio) {
    track_fragment.run.first_sample_flags = 0u;
  } else {
    fragment_run_flags.emplace_back(
        TrackFragmentRunFlags::kFirstSampleFlagsPresent);
    // The first sample in the `trun` uses the `first_sample_flags` and
    // other sample will use `default_sample_flags`.
    track_fragment.run.first_sample_flags = static_cast<uint32_t>(
        mp4::writable_boxes::FragmentSampleFlags::kSampleFlagDependsNo);
  }
  track_fragment.run.flags =
      BuildFlags<mp4::writable_boxes::TrackFragmentRunFlags>(
          fragment_run_flags);

  moof_.track_fragments[track_index] = std::move(track_fragment);
}

void Mp4MuxerDelegateFragment::AddDataToRun(
    mp4::writable_boxes::TrackFragmentRun& trun,
    const DecoderBuffer& encoded_data,
    base::TimeTicks timestamp) {
  // Additional entries may exist in various sample vectors, such as
  // durations, hence the use of 'sample_count' to ensure an accurate count of
  // valid samples.
  trun.sample_count += 1;

  // Add sample size, which is required.
  trun.sample_sizes.emplace_back(encoded_data.size());

  // Add sample timestamp.
  trun.sample_timestamps.emplace_back(timestamp);
}

void Mp4MuxerDelegateFragment::AddDataToMdat(
    std::vector<uint8_t>& track_data,
    const DecoderBuffer& encoded_data) {
  // The parameter sets are supplied in-band at the sync samples.
  // It is a default on encoded stream, see
  // `VideoEncoder::produce_annexb=false`.

  // Copy the data to the mdat.
  // TODO(crbug.com/40273983): We'll want to store the data as a vector of
  // encoded buffers instead of a single block so you don't have to resize
  // a giant blob of memory to hold them all. We should only have one
  // copy into the final muxed output buffer in an ideal world.
  size_t current_size = track_data.size();
  if (current_size + encoded_data.size() > track_data.capacity()) {
    track_data.reserve((current_size + encoded_data.size()) * 1.5);
  }

  // TODO(crbug.com/40273983): encoded stream needs to be movable container.
  track_data.resize(current_size + encoded_data.size());
  memcpy(&track_data[current_size], encoded_data.data(), encoded_data.size());
}

void Mp4MuxerDelegateFragment::AddLastTimestamp(
    mp4::writable_boxes::TrackFragmentRun& trun,
    base::TimeDelta timestamp) {
  if (trun.sample_timestamps.empty()) {
    return;
  }

  // The last sample timestamp is already added.
  if (trun.sample_timestamps.size() > trun.sample_count) {
    return;
  }

  // Use duration based on the frame rate for the last duration of the
  // last fragment.
  base::TimeTicks last_timestamp_entry = trun.sample_timestamps.back();

  trun.sample_timestamps.emplace_back(last_timestamp_entry + timestamp);
}

void Mp4MuxerDelegateFragment::SetBaseDecodeTime(
    base::TimeTicks start_audio_time,
    base::TimeTicks start_video_time) {
  if (moof_.track_fragments[kDefaultAudioIndex].run.sample_count > 0) {
    moof_.track_fragments[kDefaultAudioIndex]
        .decode_time.base_media_decode_time =
        moof_.track_fragments[kDefaultAudioIndex].run.sample_timestamps[0] -
        start_audio_time;
  }

  if (moof_.track_fragments[kDefaultVideoIndex].run.sample_count > 0) {
    moof_.track_fragments[kDefaultVideoIndex]
        .decode_time.base_media_decode_time =
        moof_.track_fragments[kDefaultVideoIndex].run.sample_timestamps[0] -
        start_video_time;
  }
}

}  // namespace media
