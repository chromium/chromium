// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_
#define MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/audio_encoder.h"
#include "media/base/video_encoder.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/mp4_muxer_context.h"
#include "media/muxers/muxer.h"

namespace media {

class AudioParameters;
class Mp4MuxerDelegateFragment;
enum VideoCodecProfile;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
class H264AnnexBToAvcBitstreamConverter;
#endif

class Mp4MuxerDelegateInterface {
 public:
  virtual ~Mp4MuxerDelegateInterface() = default;

  virtual void AddVideoFrame(
      const Muxer::VideoParameters& params,
      scoped_refptr<DecoderBuffer> encoded_data,
      std::optional<VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) = 0;

  virtual void AddAudioFrame(
      const AudioParameters& params,
      scoped_refptr<DecoderBuffer> encoded_data,
      std::optional<AudioEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) = 0;

  virtual bool Flush() = 0;

  virtual bool FlushFragment() = 0;
};

// Mp4MuxerDelegate builds the MP4 boxes from the encoded stream.
// The boxes fields will start to be populated from the first stream and
// complete in the `Flush` API call. The created box data is a complete
// MP4 format and internal data will be cleared at the end of `Flush`.
class MEDIA_EXPORT Mp4MuxerDelegate : public Mp4MuxerDelegateInterface {
 public:
  Mp4MuxerDelegate(
      AudioCodec audio_codec,
      std::optional<VideoCodecProfile> profile,
      std::optional<VideoCodecLevel> level,
      Muxer::WriteDataCB write_callback,
      size_t audio_sample_count_per_fragment = kAudioFragmentCount);
  ~Mp4MuxerDelegate() override;
  Mp4MuxerDelegate(const Mp4MuxerDelegate&) = delete;
  Mp4MuxerDelegate& operator=(const Mp4MuxerDelegate&) = delete;

  void AddVideoFrame(
      const Muxer::VideoParameters& params,
      scoped_refptr<DecoderBuffer> encoded_data,
      std::optional<VideoEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) override;

  void AddAudioFrame(
      const AudioParameters& params,
      scoped_refptr<DecoderBuffer> encoded_data,
      std::optional<AudioEncoder::CodecDescription> codec_description,
      base::TimeTicks timestamp) override;
  // Write to the big endian ISO-BMFF boxes and call `write_callback`.
  bool Flush() override;
  bool FlushFragment() override;

 private:
  void BuildFileTypeBox(mp4::writable_boxes::FileType& mp4_file_type_box);
  void BuildMovieBox();
  void BuildVideoTrackFragmentRandomAccess(
      base::TimeTicks start_timestamp,
      mp4::writable_boxes::TrackFragmentRandomAccess&
          fragment_random_access_box_writer,
      size_t written_offset);

  void BuildMovieVideoTrack(
      const Muxer::VideoParameters& params,
      const DecoderBuffer& encoded_data,
      std::optional<VideoEncoder::CodecDescription> codec_description);
  void AddDataToVideoFragment(scoped_refptr<DecoderBuffer> encoded_data);
  void BuildMovieAudioTrack(
      const AudioParameters& params,
      const DecoderBuffer& encoded_data,
      std::optional<AudioEncoder::CodecDescription> codec_description);
  void AddDataToAudioFragment(scoped_refptr<DecoderBuffer> encoded_data);

  void AddLastSampleTimestamp(int track_index, base::TimeDelta inverse_of_rate);
  int GetNextTrackIndex();
  void CreateFragmentIfNeeded(bool audio, bool is_key_frame);
  void EnsureInitialized();
  void LogBoxInfo() const;

  // The `MaybeFlushFileTypeBoxForStartup` function will be called to write the
  // file type box when the first frame is added, which makes `onstart` event
  // fired. It will return the size of the file type box.
  size_t MaybeFlushFileTypeBoxForStartup();
  size_t MaybeFlushMoovBox();
  void MaybeFlushMoofAndMfraBoxes(size_t written_offset);
  size_t GetAudioOnlyFragmentCount() const;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  scoped_refptr<DecoderBuffer> ConvertNALUData(
      scoped_refptr<DecoderBuffer> encoded_data);
#endif

  std::unique_ptr<Mp4MuxerContext> context_;
  Muxer::WriteDataCB write_callback_;

  // The MP4 has single movie box and multiple fragment boxes.
  std::unique_ptr<mp4::writable_boxes::Movie> moov_;

  // Only key video frame has `SPS` and `PPS` and it will be a
  // signal of new fragment. In Windows, key frame is every 100th frame.
  std::vector<std::unique_ptr<Mp4MuxerDelegateFragment>> fragments_;

  // video and audio index is a 0 based index that is an item of the container.
  // The track id would be plus one on this index value.
  std::optional<size_t> video_track_index_;
  std::optional<size_t> audio_track_index_;

  int next_track_index_ = 0;

  // Duration time delta for the video track.
  base::TimeTicks start_video_time_;
  base::TimeTicks last_video_time_;

  // Duration time delta for the audio track.
  base::TimeTicks start_audio_time_;
  base::TimeTicks last_audio_time_;

  double video_frame_rate_ = 0;
  int audio_sample_rate_ = 0;

  // Flush for startup is only called once.
  std::optional<size_t> written_file_type_box_size_;

  std::optional<size_t> written_mov_box_size_;

  bool live_mode_ = false;

  uint32_t sequence_number_ = 1;

  AudioCodec audio_codec_ = AudioCodec::kUnknown;
  VideoCodec video_codec_ = VideoCodec::kUnknown;

  const std::optional<media::VideoCodecProfile> video_profile_;
  const std::optional<media::VideoCodecLevel> video_level_;

  // 1000 is a count that audio samples in the same fragment
  // when no video frame is added. In Windows, when video frames are present,
  // the audio counts per fragment is much less than it.
  static constexpr uint32_t kAudioFragmentCount = 1000u;

  const size_t audio_sample_count_per_fragment_;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  std::unique_ptr<media::H264AnnexBToAvcBitstreamConverter> h264_converter_;
#endif

  Muxer::WriteDataCB write_data_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_
