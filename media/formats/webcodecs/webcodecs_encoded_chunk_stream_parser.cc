// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webcodecs/webcodecs_encoded_chunk_stream_parser.h"

#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/media_tracks.h"
#include "media/base/stream_parser_buffer.h"
#include "media/base/text_track_config.h"

namespace media {

WebCodecsEncodedChunkStreamParser::WebCodecsEncodedChunkStreamParser(
    std::unique_ptr<AudioDecoderConfig> audio_config)
    : state_(kWaitingForInit), audio_config_(std::move(audio_config)) {
  DCHECK(audio_config_ && !video_config_);
}

WebCodecsEncodedChunkStreamParser::WebCodecsEncodedChunkStreamParser(
    std::unique_ptr<VideoDecoderConfig> video_config)
    : state_(kWaitingForInit), video_config_(std::move(video_config)) {
  DCHECK(video_config_ && !audio_config_);
}

WebCodecsEncodedChunkStreamParser::~WebCodecsEncodedChunkStreamParser() =
    default;

void WebCodecsEncodedChunkStreamParser::Init(
    InitCB init_cb,
    const NewConfigCB& config_cb,
    const NewBuffersCB& new_buffers_cb,
    bool /* ignore_text_tracks */,
    const EncryptedMediaInitDataCB& /* ignored */,
    const NewMediaSegmentCB& new_segment_cb,
    const EndMediaSegmentCB& end_of_segment_cb,
    MediaLog* media_log) {
  DCHECK_EQ(state_, kWaitingForInit);
  DCHECK(!init_cb_);
  DCHECK(init_cb);
  DCHECK(config_cb);
  DCHECK(new_buffers_cb);
  DCHECK(new_segment_cb);
  DCHECK(end_of_segment_cb);

  ChangeState(kWaitingForConfigEmission);
  init_cb_ = std::move(init_cb);
  config_cb_ = config_cb;
  new_buffers_cb_ = new_buffers_cb;
  new_segment_cb_ = new_segment_cb;
  end_of_segment_cb_ = end_of_segment_cb;
  media_log_ = media_log;
}

void WebCodecsEncodedChunkStreamParser::Flush() {
  DCHECK_NE(state_, kWaitingForInit);
  if (state_ == kWaitingForEncodedChunks)
    ChangeState(kWaitingForConfigEmission);
}

bool WebCodecsEncodedChunkStreamParser::GetGenerateTimestampsFlag() const {
  return false;
}

bool WebCodecsEncodedChunkStreamParser::Parse(const uint8_t* /* buf */,
                                              int /* size */) {
  NOTREACHED();  // ProcessChunks() is the method to use instead for this
                 // parser.
  return false;
}

bool WebCodecsEncodedChunkStreamParser::ProcessChunks(
    std::unique_ptr<BufferQueue> buffer_queue) {
  // TODO(crbug.com/1144908): Implement.
  NOTIMPLEMENTED();
  return false;
}

void WebCodecsEncodedChunkStreamParser::ChangeState(State new_state) {
  DVLOG(1) << __func__ << ": " << state_ << " -> " << new_state;
  state_ = new_state;
}

}  // namespace media
