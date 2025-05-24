// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MUXER_DELEGATE_FRAGMENT_H_
#define MEDIA_MUXERS_MP4_MUXER_DELEGATE_FRAGMENT_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "media/formats/mp4/writable_box_definitions.h"

namespace media {

class Mp4MuxerContext;

template <typename T>
uint32_t BuildFlags(const std::vector<T>& build_flags) {
  uint32_t flags = 0;
  for (auto flag : build_flags) {
    flags |= static_cast<uint32_t>(flag);
  }

  return flags;
}

// It uses the default track index for audio and video regardless of the
// actual track index. Correction of the track index will be done in the
// `Finalize` function that the caller MUST call before writing
// the fragment.
inline constexpr int kDefaultAudioIndex = 0;
inline constexpr int kDefaultVideoIndex = 1;

// This class is responsible for creating and managing the fragment that holds
// audio and video data. It is also responsible for creating the moof and mdat
// boxes that will be written to the file.

class Mp4MuxerDelegateFragment {
 public:
  Mp4MuxerDelegateFragment(Mp4MuxerContext& context,
                           int video_track_id,
                           int audio_track_id,
                           uint32_t sequence_number);
  ~Mp4MuxerDelegateFragment() = default;

  size_t GetAudioSampleSize() const;
  size_t GetVideoSampleSize() const;

  void AddAudioData(scoped_refptr<DecoderBuffer> encoded_data,
                    base::TimeTicks timestamp);
  void AddVideoData(scoped_refptr<DecoderBuffer> encoded_data,
                    base::TimeTicks timestamp);
  void AddAudioLastTimestamp(base::TimeDelta timestamp);
  void AddVideoLastTimestamp(base::TimeDelta timestamp);

  base::TimeTicks GetVideoStartTimestamp() const;
  const mp4::writable_boxes::MovieFragment& GetMovieFragment() const;
  const mp4::writable_boxes::MediaData& GetMediaData() const;

  bool HasSamples() const;
  // The function will ensure that the fragment is complete and ready to be
  // written to the file.
  void Finalize(base::TimeTicks start_audio_time,
                base::TimeTicks start_video_time);

 private:
  void AddNewTrack(uint32_t track_index);
  void AddDataToRun(mp4::writable_boxes::TrackFragmentRun& trun,
                    const DecoderBuffer& encoded_data,
                    base::TimeTicks timestamp);
  void AddDataToMdat(std::vector<uint8_t>& track_data,
                     const DecoderBuffer& encoded_data);
  void AddLastTimestamp(mp4::writable_boxes::TrackFragmentRun& trun,
                        base::TimeDelta timestamp);
  void SetBaseDecodeTime(base::TimeTicks start_audio_time,
                         base::TimeTicks start_video_time);

  const raw_ref<Mp4MuxerContext> context_;
  mp4::writable_boxes::MovieFragment moof_;
  mp4::writable_boxes::MediaData mdat_;
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MUXER_DELEGATE_FRAGMENT_H_
