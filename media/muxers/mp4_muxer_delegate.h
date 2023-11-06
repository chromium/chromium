// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_
#define MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/audio_encoder.h"
#include "media/base/video_encoder.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/mp4_muxer_context.h"
#include "media/muxers/muxer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

class AudioParameters;

class Mp4MuxerDelegateInterface {
 public:
  virtual ~Mp4MuxerDelegateInterface() = default;

  virtual void AddVideoFrame(
      const Muxer::VideoParameters& params,
      std::string encoded_data,
      absl::optional<VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp,
      bool is_key_frame) = 0;

  virtual void AddAudioFrame(
      const AudioParameters& params,
      std::string encoded_data,
      absl::optional<AudioEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) = 0;

  virtual bool Flush() = 0;
};

// Mp4MuxerDelegate builds the MP4 boxes from the encoded stream.
// The boxes fields will start to be populated from the first stream and
// complete in the `Flush` API call. The created box data is a complete
// MP4 format and internal data will be cleared at the end of `Flush`.
class MEDIA_EXPORT Mp4MuxerDelegate : public Mp4MuxerDelegateInterface {
 public:
  explicit Mp4MuxerDelegate(
      Muxer::WriteDataCB write_callback,
      base::TimeDelta max_audio_only_fragment_duration = base::Seconds(5));
  ~Mp4MuxerDelegate() override;
  Mp4MuxerDelegate(const Mp4MuxerDelegate&) = delete;
  Mp4MuxerDelegate& operator=(const Mp4MuxerDelegate&) = delete;

  void AddVideoFrame(
      const Muxer::VideoParameters& params,
      std::string encoded_data,
      absl::optional<VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp,
      bool is_key_frame) override;

  void AddAudioFrame(
      const AudioParameters& params,
      std::string encoded_data,
      absl::optional<AudioEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) override;
  // Write to the big endian ISO-BMFF boxes and call `write_callback`.
  bool Flush() override;

  struct Fragment {
    Fragment();
    ~Fragment() = default;
    Fragment(const Fragment&) = delete;
    Fragment& operator=(const Fragment&) = delete;

    mp4::writable_boxes::MovieFragment moof;
    mp4::writable_boxes::MediaData mdat;
  };

 private:
  void BuildFileTypeBox(mp4::writable_boxes::FileType& mp4_file_type_box);
  void BuildMovieBox();
  void BuildVideoTrackFragmentRandomAccess(
      mp4::writable_boxes::TrackFragmentRandomAccess&
          fragment_random_access_box_writer,
      Fragment& fragment,
      size_t written_offset);

  void BuildVideoTrackWithKeyframe(
      const Muxer::VideoParameters& params,
      std::string encoded_data,
      VideoEncoder::CodecDescription codec_description);
  void BuildVideoFragment(std::string encoded_data, bool is_key_frame);
  void BuildAudioTrack(const AudioParameters& params,
                       std::string encoded_data,
                       AudioEncoder::CodecDescription codec_description);
  void BuildAudioFragment(std::string encoded_data);

  void AddLastSampleTimestamp(int track_index, base::TimeDelta inverse_of_rate);
  int GetNextTrackIndex();
  void EnsureInitialized();
  void Reset();
  void LogBoxInfo() const;

  std::unique_ptr<Mp4MuxerContext> context_;
  Muxer::WriteDataCB write_callback_;

  // The MP4 has single movie box and multiple fragment boxes.
  std::unique_ptr<mp4::writable_boxes::Movie> moov_;

  // Only key video frame has `SPS` and `PPS` and it will be a
  // signal of new fragment. In Windows, key frame is every 100th frame.
  std::vector<std::unique_ptr<Fragment>> fragments_;

  // video and audio index is a 0 based index that is an item of the container.
  // The track id would be plus one on this index value.
  absl::optional<size_t> video_track_index_;
  absl::optional<size_t> audio_track_index_;
  int next_track_index_ = 0;

  // Duration time delta for the video track.
  base::TimeTicks start_video_time_;
  base::TimeTicks last_video_time_;

  // Duration time delta for the audio track.
  base::TimeTicks start_audio_time_;
  base::TimeTicks last_audio_time_;

  double video_frame_rate_ = 0;
  int audio_sample_rate_ = 0;

  base::TimeDelta max_audio_only_fragment_duration_;

  Muxer::WriteDataCB write_data_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_
