// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_MP4_STREAM_PARSER_H_
#define MEDIA_FORMATS_MP4_MP4_STREAM_PARSER_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/formats/common/offset_byte_queue.h"
#include "media/formats/mp4/parse_result.h"
#include "media/formats/mp4/track_run_iterator.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/formats/mp4/aac.h"
#endif

namespace media::mp4 {

struct Movie;
struct MovieHeader;
struct TrackHeader;
class BoxReader;

class MEDIA_EXPORT MP4StreamParser : public StreamParser {
 public:
  MP4StreamParser(std::optional<base::flat_set<int>> strict_audio_object_types,
                  bool has_sbr,
                  bool has_flac,
                  bool has_iamf,
                  bool has_dv);

  MP4StreamParser(const MP4StreamParser&) = delete;
  MP4StreamParser& operator=(const MP4StreamParser&) = delete;

  ~MP4StreamParser() override;

  void Init(InitCB init_cb,
            NewConfigCB config_cb,
            NewBuffersCB new_buffers_cb,
            EncryptedMediaInitDataCB encrypted_media_init_data_cb,
            NewMediaSegmentCB new_segment_cb,
            EndMediaSegmentCB end_of_segment_cb,
            MediaLog* media_log) override;
  void Flush() override;
  bool GetGenerateTimestampsFlag() const override;
  [[nodiscard]] bool AppendToParseBuffer(
      base::span<const uint8_t> buf) override;
  [[nodiscard]] ParseStatus Parse(int max_pending_bytes_to_inspect) override;

  // Calculates the rotation value from the track header display matricies.
  VideoTransformation CalculateRotation(const TrackHeader& track,
                                        const MovieHeader& movie);

 private:
  enum State {
    kWaitingForInit,
    kParsingBoxes,
    kWaitingForSampleData,
    kEmittingSamples,
    kError
  };

  // Wrappers of `queue_` that observe constraint of `max_parse_offset_`.
  void ModulatedPeek(const uint8_t** buf, int* size);
  void ModulatedPeekAt(int64_t offset, const uint8_t** buf, int* size);
  bool ModulatedTrim(int64_t max_offset);

  ParseResult ParseBox();
  bool ParseMoov(mp4::BoxReader* reader);
  bool ParseMoof(mp4::BoxReader* reader);

  void OnEncryptedMediaInitData(
      const std::vector<ProtectionSystemSpecificHeader>& headers);

  // To retain proper framing, each 'mdat' atom must be read; to limit memory
  // usage, the atom's data needs to be discarded incrementally as frames are
  // extracted from the stream. This function discards data from the stream up
  // to |max_clear_offset|, updating the |mdat_tail_| value so that framing can
  // be retained after all 'mdat' information has been read. |max_clear_offset|
  // is the upper bound on what can be removed from |queue_|. Anything below
  // this offset is no longer needed by the parser.
  // Returns 'true' on success, 'false' if there was an error.
  bool ReadAndDiscardMDATsUntil(int64_t max_clear_offset);

  void ChangeState(State new_state);

  bool EmitConfigs();
  ParseResult EnqueueSample(BufferQueueMap* buffers);
  bool SendAndFlushSamples(BufferQueueMap* buffers);

  void Reset();

  // Checks to see if we have enough data in |queue_| to transition to
  // kEmittingSamples and start enqueuing samples.
  bool HaveEnoughDataToEnqueueSamples();

  // Sets |highest_end_offset_| based on the data in |moov_|
  // and |moof|. Returns true if |highest_end_offset_| was successfully
  // computed.
  bool ComputeHighestEndOffset(const MovieFragment& moof);

  State state_;
  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  EncryptedMediaInitDataCB encrypted_media_init_data_cb_;
  NewMediaSegmentCB new_segment_cb_;
  EndMediaSegmentCB end_of_segment_cb_;
  raw_ptr<MediaLog> media_log_;

  // Bytes of the mp4 stream.
  // `max_parse_offset_` tracks the point in `queue_` beyond which no data may
  // yet be parsed even if it is less than the queue's tail offset. This allows
  // incremental parsing. `max_parse_offset_` must be less than or equal to the
  // queue_'s current tail offset. Note that operations like Trim() and PeekAt()
  // on the offset queue can involve offsets beyond tail or `max_parse_offset_`,
  // so this parser must consider `max_parse_offset_` too when using those
  // operations, otherwise more data than the amount indicated in the Parse()
  // call's `max_pending_bytes_to_inspect` increment might be inspected in a
  // Parse() call. See the various Modulated*() wrappers in this class.
  // TODO(crbug.com/40815633): Consider reworking all these parsers to
  // use a new type of queue that internally modulates the increment.
  int64_t max_parse_offset_ = 0;
  OffsetByteQueue queue_;

  // These two parameters are only valid in the |kEmittingSegments| state.
  //
  // |moof_head_| is the offset of the start of the most recently parsed moof
  // block. All byte offsets in sample information are relative to this offset,
  // as mandated by the Media Source spec.
  int64_t moof_head_;
  // |mdat_tail_| is the stream offset of the end of the current 'mdat' box.
  // Valid iff it is greater than the head of the queue.
  int64_t mdat_tail_;

  // The highest end offset in the current moof. This offset is
  // relative to |moof_head_|. This value is used to make sure we have collected
  // enough bytes to parse all samples and aux_info in the current moof.
  int64_t highest_end_offset_;

  std::unique_ptr<mp4::Movie> moov_;
  std::unique_ptr<mp4::TrackRunIterator> runs_;

  bool has_audio_;
  bool has_video_;
  std::set<uint32_t> audio_track_ids_;
  std::set<uint32_t> video_track_ids_;

  // The object types allowed for audio tracks. For FLAC indication, use
  // |has_flac_|. If this is a nullopt, then strict object type assertion will
  // not happen.
  const std::optional<base::flat_set<int>> strict_audio_object_types_;
  const bool has_sbr_;
  const bool has_flac_;
  const bool has_iamf_;
  // Indicate if source buffer has been set as Dolby Vision. If true,
  // always treat the source buffer as Dolby Vision, if false and if
  // the source buffer is cross-compatible, use its compatible codec
  // defined in Dolby Vision Profiles and Levels specification:
  // https://professionalsupport.dolby.com/s/article/What-is-Dolby-Vision-Profile,
  // otherwise still treat the buffer as Dolby Vision.
  const bool has_dv_;

  // Tracks the number of MEDIA_LOGS for skipping empty trun samples.
  int num_empty_samples_skipped_;

  // Tracks the number of MEDIA_LOGS for invalid bitstream conversion.
  int num_invalid_conversions_;

  // Tracks the number of MEDIA_LOGS for video keyframe MP4<->frame mismatch.
  int num_video_keyframe_mismatches_;
};

}  // namespace media::mp4

#endif  // MEDIA_FORMATS_MP4_MP4_STREAM_PARSER_H_
