// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBCODECS_WEBCODECS_ENCODED_CHUNK_STREAM_PARSER_H_
#define MEDIA_FORMATS_WEBCODECS_WEBCODECS_ENCODED_CHUNK_STREAM_PARSER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/base/video_decoder_config.h"

namespace media {

class MEDIA_EXPORT WebCodecsEncodedChunkStreamParser : public StreamParser {
 public:
  explicit WebCodecsEncodedChunkStreamParser(
      std::unique_ptr<AudioDecoderConfig> audio_config);
  explicit WebCodecsEncodedChunkStreamParser(
      std::unique_ptr<VideoDecoderConfig> video_config);

  WebCodecsEncodedChunkStreamParser(const WebCodecsEncodedChunkStreamParser&) =
      delete;
  WebCodecsEncodedChunkStreamParser& operator=(
      const WebCodecsEncodedChunkStreamParser&) = delete;

  ~WebCodecsEncodedChunkStreamParser() override;

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
  [[nodiscard]] ParseStatus Parse(int max_pending_bytes_to_expect) override;

  // Processes and emits buffers from |buffer_queue|. If state is
  // kWaitingForConfigEmission, first emit the config.
  bool ProcessChunks(std::unique_ptr<BufferQueue> buffer_queue) override;

 private:
  enum State {
    kWaitingForInit,
    kWaitingForConfigEmission,
    kWaitingForEncodedChunks,
    kError
  };

  void ChangeState(State new_state);

  State state_;

  // These configs are populated during ctor. A copy of the appropriate config
  // is emitted on demand when "parsing" newly appended encoded chunks if that
  // append occurs when state is kWaitingForConfigEmission. Note, only one type
  // of config can be emitted (not both), for an instance of this parser.
  std::unique_ptr<AudioDecoderConfig> audio_config_;
  std::unique_ptr<VideoDecoderConfig> video_config_;

  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;

  NewMediaSegmentCB new_segment_cb_;
  EndMediaSegmentCB end_of_segment_cb_;
  raw_ptr<MediaLog> media_log_;
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBCODECS_WEBCODECS_ENCODED_CHUNK_STREAM_PARSER_H_
