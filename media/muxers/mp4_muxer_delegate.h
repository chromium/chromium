// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_
#define MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_

#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/audio_encoder.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/writable_box_definitions.h"
#include "media/muxers/muxer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

class AudioParameters;
class Mp4MuxerNaluReader;

// Mp4MuxerDelegate builds the MP4 boxes from the encoded stream.
// The boxes fields will start to be populated from the first stream and
// complete in the `Flush` API call. The created box data is a complete
// MP4 format and internal data will be cleared at the end of `Flush`.
class MEDIA_EXPORT Mp4MuxerDelegate {
 public:
  explicit Mp4MuxerDelegate(Muxer::WriteDataCB write_callback);
  ~Mp4MuxerDelegate();
  Mp4MuxerDelegate(const Mp4MuxerDelegate&) = delete;
  Mp4MuxerDelegate& operator=(const Mp4MuxerDelegate&) = delete;

  void AddVideoFrame(const Muxer::VideoParameters& params,
                     base::StringPiece encoded_data,
                     base::TimeTicks timestamp);

  void AddAudioFrame(const AudioParameters& params,
                     base::StringPiece encoded_data,
                     const AudioEncoder::CodecDescription& codec_description,
                     base::TimeTicks timestamp);
  // Write to the big endian ISO-BMFF boxes and call `write_callback`.
  void Flush();

 private:
  struct Fragment {
    Fragment();
    ~Fragment() = default;
    Fragment(const Fragment&) = delete;
    Fragment& operator=(const Fragment&) = delete;

    mp4::writable_boxes::MovieFragment moof;
    mp4::writable_boxes::MediaData mdat;
  };

  void PopulateMovieHeader();
  void PopulateInitialVideoTrack(const Muxer::VideoParameters& params,
                                 base::StringPiece encoded_data,
                                 int index);
  void PopulateVideoFragment(const Muxer::VideoParameters& params,
                             base::StringPiece encoded_data,
                             base::TimeTicks timestamp);
  void AddSampleDataToTrunAndMdat(mp4::writable_boxes::TrackFragmentRun& trun,
                                  Mp4MuxerDelegate::Fragment* fragment,
                                  Mp4MuxerNaluReader& nalu_reader,
                                  base::StringPiece encoded_data,
                                  base::TimeTicks timestamp,
                                  uint32_t timescale);
  void AddSampleDuration(mp4::writable_boxes::TrackFragmentRun& trun,
                         base::TimeTicks timestamp,
                         uint32_t timescale);
  int GetNextTrackIndex();

  // The MP4 has single movie box and multiple fragment boxes.
  mp4::writable_boxes::Movie movie_box_;

  // Only key video frame has `SPS` and `PPS` and it will be a
  // signal of new fragment. In Windows, key frame is every 100th frame.
  std::vector<std::unique_ptr<Fragment>> fragments_;

  // video and audio index is a 0 based index that is an item of the container.
  // The track id would be plus one on this index value.
  int video_track_index_ = -1;
  int next_track_index_ = 0;
  std::vector<base::TimeTicks> video_captured_time_;

  Muxer::WriteDataCB write_data_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_
