// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_ES_PARSER_H264_H_
#define MEDIA_FORMATS_MP2T_ES_PARSER_H264_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "media/base/media_export.h"
#include "media/base/ranges.h"
#include "media/base/video_decoder_config.h"
#include "media/formats/mp2t/es_adapter_video.h"
#include "media/formats/mp2t/es_parser.h"
#include "media/media_buildflags.h"

namespace media {
class H264Parser;
struct H264SPS;
}

namespace media {
namespace mp2t {

// A few remarks:
// - In this h264 parser, frame splitting is based on AUD nals.
// Mpeg2 TS spec: "2.14 Carriage of Rec. ITU-T H.264 | ISO/IEC 14496-10 video"
// "Each AVC access unit shall contain an access unit delimiter NAL Unit;"
// - PES packets do not necessarily map to an H264 access unit although the HLS
// recommendation is to use one PES for each access unit. In this parser,
// we handle the general case and do not make any assumption about the access
// unit organization within PES packets.
//
class MEDIA_EXPORT EsParserH264 : public EsParser {
 public:
  using NewVideoConfigCB =
      base::RepeatingCallback<void(const VideoDecoderConfig&)>;

  EsParserH264(NewVideoConfigCB new_video_config_cb,
               EmitBufferCB emit_buffer_cb);
  EsParserH264(NewVideoConfigCB new_video_config_cb,
               EmitBufferCB emit_buffer_cb,
               EncryptionScheme init_encryption_scheme,
               const GetDecryptConfigCB& get_decrypt_config_cb);

  EsParserH264(const EsParserH264&) = delete;
  EsParserH264& operator=(const EsParserH264&) = delete;

  ~EsParserH264() override;

  // EsParser implementation.
  void Flush() override;

 private:
  // EsParser implementation.
  bool ParseFromEsQueue() override;
  void ResetInternal() override;

  // Find the AUD located at or after |*stream_pos|.
  // Return true if an AUD is found.
  // If found, |*stream_pos| corresponds to the position of the AUD start code
  // in the stream. Otherwise, |*stream_pos| corresponds to the last position
  // of the start code parser.
  bool FindAUD(int64_t* stream_pos);

  // Emit a frame whose position in the ES queue starts at |access_unit_pos|.
  // Returns true if successful, false if no PTS is available for the frame.
  bool EmitFrame(int64_t access_unit_pos,
                 int access_unit_size,
                 bool is_key_frame,
                 int pps_id);

  // Update the video decoder config based on an H264 SPS.
  // Return true if successful.
  bool UpdateVideoDecoderConfig(const H264SPS* sps, EncryptionScheme scheme);

  EsAdapterVideo es_adapter_;

  // H264 parser state.
  // - |current_access_unit_pos_| is pointing to an annexB syncword
  // representing the first NALU of an H264 access unit.
  std::unique_ptr<H264Parser> h264_parser_;
  int64_t current_access_unit_pos_;
  int64_t next_access_unit_pos_;
  const EncryptionScheme init_encryption_scheme_;
  // Callback to obtain the current decrypt_config.
  GetDecryptConfigCB get_decrypt_config_cb_;
  Ranges<int> protected_blocks_;

  // Last video decoder config.
  VideoDecoderConfig last_video_decoder_config_;
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_ES_PARSER_H264_H_
