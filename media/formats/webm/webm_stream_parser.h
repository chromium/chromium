// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_WEBM_WEBM_STREAM_PARSER_H_
#define MEDIA_FORMATS_WEBM_WEBM_STREAM_PARSER_H_

#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/byte_queue.h"
#include "media/base/media_export.h"
#include "media/base/stream_parser.h"
#include "media/base/video_decoder_config.h"

namespace media {

class WebMClusterParser;

class MEDIA_EXPORT WebMStreamParser : public StreamParser {
 public:
  WebMStreamParser();
  ~WebMStreamParser() override;

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

 private:
  enum State {
    kWaitingForInit,
    kParsingHeaders,
    kParsingClusters,
    kError
  };

  void ChangeState(State new_state);

  // Parses WebM Header, Info, Tracks elements. It also skips other level 1
  // elements that are not used right now. Once the Info & Tracks elements have
  // been parsed, this method will transition the parser from PARSING_HEADERS to
  // PARSING_CLUSTERS.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseInfoAndTracks(const uint8_t* data, int size);

  // Incrementally parses WebM cluster elements. This method also skips
  // CUES elements if they are encountered since we currently don't use the
  // data in these elements.
  //
  // Returns < 0 if the parse fails.
  // Returns 0 if more data is needed.
  // Returning > 0 indicates success & the number of bytes parsed.
  int ParseCluster(const uint8_t* data, int size);

  // Fire the encrypted event through the |encrypted_media_init_data_cb_|.
  void OnEncryptedMediaInitData(const std::string& key_id);

  State state_;
  InitCB init_cb_;
  NewConfigCB config_cb_;
  NewBuffersCB new_buffers_cb_;
  bool ignore_text_tracks_;
  EncryptedMediaInitDataCB encrypted_media_init_data_cb_;

  NewMediaSegmentCB new_segment_cb_;
  EndMediaSegmentCB end_of_segment_cb_;
  MediaLog* media_log_;

  bool unknown_segment_size_;

  std::unique_ptr<WebMClusterParser> cluster_parser_;
  ByteQueue byte_queue_;

  DISALLOW_COPY_AND_ASSIGN(WebMStreamParser);
};

}  // namespace media

#endif  // MEDIA_FORMATS_WEBM_WEBM_STREAM_PARSER_H_
