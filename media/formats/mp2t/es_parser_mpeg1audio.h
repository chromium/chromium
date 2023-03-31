// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_MPEG1AUDIO_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_MPEG1AUDIO_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/formats/mp2t/es_parser.h"

namespace media {
class AudioTimestampHelper;
}

namespace media {
namespace mp2t {

class MEDIA_EXPORT EsParserMpeg1Audio : public EsParser {
 public:
  using NewAudioConfigCB =
      base::RepeatingCallback<void(const AudioDecoderConfig&)>;

  EsParserMpeg1Audio(const NewAudioConfigCB& new_audio_config_cb,
                     EmitBufferCB emit_buffer_cb,
                     MediaLog* media_log);

  EsParserMpeg1Audio(const EsParserMpeg1Audio&) = delete;
  EsParserMpeg1Audio& operator=(const EsParserMpeg1Audio&) = delete;

  ~EsParserMpeg1Audio() override;

  // EsParser implementation.
  void Flush() override;

 private:
  // Used to link a PTS with a byte position in the ES stream.
  typedef std::pair<int64_t, base::TimeDelta> EsPts;
  typedef std::list<EsPts> EsPtsList;

  struct Mpeg1AudioFrame;

  // EsParser implementation.
  bool ParseFromEsQueue() override;
  void ResetInternal() override;

  // Synchronize the stream on a Mpeg1 audio syncword (consuming bytes from
  // |es_queue_| if needed).
  // Returns true when a full Mpeg1 audio frame has been found: in that case
  // |mpeg1audio_frame| structure is filled up accordingly.
  // Returns false otherwise (no Mpeg1 audio syncword found or partial Mpeg1
  // audio frame).
  bool LookForMpeg1AudioFrame(Mpeg1AudioFrame* mpeg1audio_frame);

  // Signal any audio configuration change (if any).
  // Return false if the current audio config is not
  // a supported Mpeg1 audio config.
  bool UpdateAudioConfiguration(const uint8_t* mpeg1audio_header);

  void SkipMpeg1AudioFrame(const Mpeg1AudioFrame& mpeg1audio_frame);

  raw_ptr<MediaLog> media_log_;

  size_t mp3_parse_error_limit_ = 0;

  // Callbacks:
  // - to signal a new audio configuration,
  // - to send ES buffers.
  NewAudioConfigCB new_audio_config_cb_;
  EmitBufferCB emit_buffer_cb_;

  // Interpolated PTS for frames that don't have one.
  std::unique_ptr<AudioTimestampHelper> audio_timestamp_helper_;

  // Last audio config.
  AudioDecoderConfig last_audio_decoder_config_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_ES_PARSER_MPEG1AUDIO_H_
