// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_MPEG_AUDIO_STREAM_PARSER_BASE_H_
#define MEDIA_FORMATS_MPEG_MPEG_AUDIO_STREAM_PARSER_BASE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bit_reader.h"
#include "media/base/byte_queue.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"

namespace media {

class MEDIA_EXPORT MPEGAudioStreamParserBase : public StreamParser {
 public:
  // |start_code_mask| is used to find the start of each frame header.  Also
  // referred to as the sync code in the MP3 and ADTS header specifications.
  // |codec_delay| is the number of samples the decoder will output before the
  // first real frame.
  MPEGAudioStreamParserBase(uint32_t start_code_mask,
                            AudioCodec audio_codec,
                            int codec_delay);
  ~MPEGAudioStreamParserBase() override;

  // StreamParser implementation.
  void Init(InitCB init_cb,
            const NewConfigCB& config_cb,
            const NewBuffersCB& new_buffers_cb,
            bool ignore_text_tracks,
            const EncryptedMediaInitDataCB& encrypted_media_init_data_cb,
            const NewMediaSegmentCB& new_segment_cb,
            const EndMediaSegmentCB& end_of_segment_cb,
            MediaLog* media_log) override;
  void Flush() override;
  bool GetGenerateTimestampsFlag() const override;
  bool Parse(const uint8_t* buf, int size) override;

 protected:
  // Subclasses implement this method to parse format specific frame headers.
  // |data| & |size| describe the data available for parsing.
  //
  // Implementations are expected to consume an entire frame header.  It should
  // only return a value greater than 0 when |data| has enough bytes to
  // successfully parse & consume the entire frame header.
  //
  // |frame_size| - Required parameter that is set to the size of the frame, in
  // bytes, including the frame header if the function returns a value > 0.
  // |sample_rate| - Optional parameter that is set to the sample rate
  // of the frame if this function returns a value > 0.
  // |channel_layout| - Optional parameter that is set to the channel_layout
  // of the frame if this function returns a value > 0.
  // |sample_count| - Optional parameter that is set to the number of samples
  // in the frame if this function returns a value > 0.
  // |metadata_frame| - Optional parameter that is set to true if the frame has
  // valid values for the above parameters, but no usable encoded data; only set
  // to true if this function returns a value > 0.
  //
  // |sample_rate|, |channel_layout|, |sample_count|, |metadata_frame| may be
  // NULL if the caller is not interested in receiving these values from the
  // frame header.
  //
  // If |metadata_frame| is true, the MPEGAudioStreamParserBase will discard the
  // frame after consuming the metadata values above.
  //
  // Returns:
  // > 0 : The number of bytes parsed.
  //   0 : If more data is needed to parse the entire frame header.
  // < 0 : An error was encountered during parsing.
  virtual int ParseFrameHeader(const uint8_t* data,
                               int size,
                               int* frame_size,
                               int* sample_rate,
                               ChannelLayout* channel_layout,
                               int* sample_count,
                               bool* metadata_frame,
                               std::vector<uint8_t>* extra_data) = 0;

  MediaLog* media_log() const { return media_log_; }

 private:
  enum State {
    UNINITIALIZED,
    INITIALIZED,
    PARSE_ERROR
  };

  void ChangeState(State state);

  // Parsing functions for various byte stream elements.  |data| & |size|
  // describe the data available for parsing.
  //
  // Returns:
  // > 0 : The number of bytes parsed.
  //   0 : If more data is needed to parse the entire element.
  // < 0 : An error was encountered during parsing.
  int ParseFrame(const uint8_t* data, int size, BufferQueue* buffers);
  int ParseIcecastHeader(const uint8_t* data, int size);
  int ParseID3v1(const uint8_t* data, int size);
  int ParseID3v2(const uint8_t* data, int size);

  // Parses an ID3v2 "sync safe" integer.
  // |reader| - A BitReader to read from.
  // |value| - Set to the integer value read, if true is returned.
  //
  // Returns true if the integer was successfully parsed and |value|
  // was set.
  // Returns false if an error was encountered. The state of |value| is
  // undefined when false is returned.
  bool ParseSyncSafeInt(BitReader* reader, int32_t* value);

  // Scans |data| for the next valid start code.
  // Returns:
  // > 0 : The number of bytes that should be skipped to reach the
  //       next start code..
  //   0 : If a valid start code was not found and more data is needed.
  // < 0 : An error was encountered during parsing.
  int FindNextValidStartCode(const uint8_t* data, int size);

  // Sends the buffers in |buffers| to |new_buffers_cb_| and then clears
  // |buffers|.
  // If |end_of_segment| is set to true, then |end_of_segment_cb_| is called
  // after |new_buffers_cb_| to signal that these buffers represent the end of a
  // media segment.
  // Returns true if the buffers are sent successfully.
  bool SendBuffers(BufferQueue* buffers, bool end_of_segment);

  State state_;

  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  NewMediaSegmentCB new_segment_cb_;
  EndMediaSegmentCB end_of_segment_cb_;
  MediaLog* media_log_;

  ByteQueue queue_;

  AudioDecoderConfig config_;
  std::unique_ptr<AudioTimestampHelper> timestamp_helper_;
  bool in_media_segment_;
  const uint32_t start_code_mask_;
  const AudioCodec audio_codec_;
  const int codec_delay_;

  DISALLOW_COPY_AND_ASSIGN(MPEGAudioStreamParserBase);
};

}  // namespace media

#endif  // MEDIA_FORMATS_MPEG_MPEG_AUDIO_STREAM_PARSER_BASE_H_
