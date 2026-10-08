// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_MPEG_AUDIO_STREAM_PARSER_BASE_H_
#define MEDIA_FORMATS_MPEG_MPEG_AUDIO_STREAM_PARSER_BASE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/byte_queue.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"

namespace media {

namespace formats::mpeg {
struct MpegAudioHeaderInfo;
}  // namespace formats::mpeg

class MEDIA_EXPORT MPEGAudioStreamParserBase : public StreamParser {
 public:
  struct Header {
    // Size of the frame in bytes, including the frame header.
    size_t frame_size = 0;

    // Sample rate of the frame in Hz.
    int sample_rate = 0;

    // Channel layout configuration.
    ChannelLayout channel_layout = CHANNEL_LAYOUT_NONE;

    // Number of samples per frame.
    int sample_count = 0;

    // Set to true if the frame has valid header values, but no usable
    // encoded data (e.g., silent XING metadata frames). If true, the
    // base parser will discard the frame after consuming its metadata.
    bool metadata_frame = false;

    // Subclass-specific extra data configurations (e.g., AudioSpecificConfig).
    std::vector<uint8_t> extra_data;
  };
  // |codec_delay| is the number of samples the decoder will output before the
  // first real frame.
  MPEGAudioStreamParserBase(AudioCodec audio_codec, int codec_delay);

  MPEGAudioStreamParserBase(const MPEGAudioStreamParserBase&) = delete;
  MPEGAudioStreamParserBase& operator=(const MPEGAudioStreamParserBase&) =
      delete;

  ~MPEGAudioStreamParserBase() override;

  // StreamParser implementation.
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

 protected:
  // Converts a Rust FFI header info struct into the C++ parser Header struct.
  virtual Header FfiHeaderToHeader(
      const formats::mpeg::MpegAudioHeaderInfo& ffi_header) const = 0;

 private:
  enum State {
    UNINITIALIZED,
    INITIALIZED,
    PARSE_ERROR
  };

  bool ProcessAudioFrame(const Header& header,
                         base::span<const uint8_t> data,
                         BufferQueue* buffers);

  void ChangeState(State state);

  // Sends the buffers in |buffers| to |new_buffers_cb_| and then clears
  // |buffers|.
  // If |end_of_segment| is set to true, then |end_of_segment_cb_| is called
  // after |new_buffers_cb_| to signal that these buffers represent the end of a
  // media segment.
  // Returns true if the buffers are sent successfully.
  bool SendBuffers(BufferQueue* buffers, bool end_of_segment);

  State state_ = UNINITIALIZED;

  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  NewMediaSegmentCB new_segment_cb_;
  EndMediaSegmentCB end_of_segment_cb_;

  // Tracks how much data has not yet been attempted to be parsed from `queue_`
  // between calls to Parse(). AppendToParseBuffer() increases this from 0 as
  // more data is added. Parse() incrementally reduces this and Flush() zeroes
  // this. Note that Parse() may have inspected some data at the front of
  // `queue_` but not yet been able to pop it from the queue. So this value may
  // be lower than the actual amount of bytes in `queue_`, since more data is
  // needed to complete the parse.
  int uninspected_pending_bytes_ = 0;
  ByteQueue queue_;

  AudioDecoderConfig config_;
  std::unique_ptr<AudioTimestampHelper> timestamp_helper_;
  bool in_media_segment_ = false;
  const AudioCodec audio_codec_;
  const int codec_delay_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_MPEG_MPEG_AUDIO_STREAM_PARSER_BASE_H_
