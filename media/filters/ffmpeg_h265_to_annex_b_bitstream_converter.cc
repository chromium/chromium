// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/ffmpeg_h265_to_annex_b_bitstream_converter.h"

#include <stdint.h>

#include "base/logging.h"
#include "media/base/decrypt_config.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/formats/mp4/avc.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/hevc.h"

namespace media {

FFmpegH265ToAnnexBBitstreamConverter::FFmpegH265ToAnnexBBitstreamConverter(
    AVCodecParameters* stream_codec_parameters)
    : stream_codec_parameters_(stream_codec_parameters) {
  CHECK(stream_codec_parameters_);
}

FFmpegH265ToAnnexBBitstreamConverter::~FFmpegH265ToAnnexBBitstreamConverter() {}

bool FFmpegH265ToAnnexBBitstreamConverter::ConvertPacket(AVPacket* packet) {
  DVLOG(3) << __func__;
  if (packet == NULL || !packet->data)
    return false;

  // Calculate the needed output buffer size.
  if (!hevc_config_) {
    if (!stream_codec_parameters_->extradata ||
        stream_codec_parameters_->extradata_size <= 0) {
      DVLOG(1) << "HEVCDecoderConfiguration not found, no extra codec data";
      return false;
    }

    hevc_config_ = std::make_unique<mp4::HEVCDecoderConfigurationRecord>();

    if (!hevc_config_->Parse(stream_codec_parameters_->extradata,
                             stream_codec_parameters_->extradata_size)) {
      DVLOG(1) << "Parsing HEVCDecoderConfiguration failed";
      return false;
    }
  }

  std::vector<uint8_t> input_frame;
  std::vector<SubsampleEntry> subsamples;
  // TODO(servolk): Performance could be improved here, by reducing unnecessary
  // data copying, but first annex b conversion code needs to be refactored to
  // allow that (see crbug.com/455379).
  input_frame.insert(input_frame.end(),
                     packet->data, packet->data + packet->size);
  size_t nalu_size_len = hevc_config_->lengthSizeMinusOne + 1;
  if (!mp4::AVC::ConvertFrameToAnnexB(nalu_size_len, &input_frame,
                                      &subsamples)) {
    DVLOG(1) << "AnnexB conversion failed";
    return false;
  }

  if (packet->flags & AV_PKT_FLAG_KEY) {
    RCHECK(mp4::HEVC::InsertParamSetsAnnexB(*hevc_config_.get(),
                                            &input_frame, &subsamples));
    DVLOG(4) << "Inserted HEVC decoder params";
  }

  uint32_t output_packet_size = input_frame.size();

  if (output_packet_size == 0)
    return false;  // Invalid input packet.

  // Allocate new packet for the output.
  AVPacket dest_packet;
  if (av_new_packet(&dest_packet, output_packet_size) != 0)
    return false;  // Memory allocation failure.

  // This is a bit tricky: since the interface does not allow us to replace
  // the pointer of the old packet with a new one, we will initially copy the
  // metadata from old packet to new bigger packet.
  av_packet_copy_props(&dest_packet, packet);

  // Proceed with the conversion of the actual in-band NAL units, leave room
  // for configuration in the beginning.
  memcpy(dest_packet.data, &input_frame[0], input_frame.size());

  // At the end we must destroy the old packet.
  av_packet_unref(packet);

  // Finally, replace the values in the input packet.
  memcpy(packet, &dest_packet, sizeof(*packet));
  return true;
}

}  // namespace media
