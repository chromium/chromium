// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_h264_to_annex_b_bitstream_converter.h"

#include <stdint.h>

#include "base/logging.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {

FFmpegH264ToAnnexBBitstreamConverter::FFmpegH264ToAnnexBBitstreamConverter(
    AVCodecParameters* stream_codec_parameters)
    : configuration_processed_(false),
      stream_codec_parameters_(stream_codec_parameters) {
  CHECK(stream_codec_parameters_);
}

FFmpegH264ToAnnexBBitstreamConverter::~FFmpegH264ToAnnexBBitstreamConverter() =
    default;

bool FFmpegH264ToAnnexBBitstreamConverter::ConvertPacket(AVPacket* packet) {
  std::unique_ptr<mp4::AVCDecoderConfigurationRecord> avc_config;

  if (packet == NULL || !packet->data) {
    DVLOG(2) << __func__ << ": Null or empty packet";
    return false;
  }

  // Calculate the needed output buffer size.
  if (!configuration_processed_) {
    if (!stream_codec_parameters_->extradata ||
        stream_codec_parameters_->extradata_size <= 0) {
      DVLOG(2) << __func__ << ": Empty extra data";
      return false;
    }

    avc_config = std::make_unique<mp4::AVCDecoderConfigurationRecord>();

    if (!converter_.ParseConfiguration(stream_codec_parameters_->extradata,
                                       stream_codec_parameters_->extradata_size,
                                       avc_config.get())) {
      DVLOG(2) << __func__ << ": ParseConfiguration() failure";
      return false;
    }
  }

  uint32_t output_packet_size = converter_.CalculateNeededOutputBufferSize(
      packet->data, packet->size, avc_config.get());

  if (output_packet_size == 0) {
    DVLOG(2) << __func__ << ": zero |output_packet_size|";
    return false;  // Invalid input packet.
  }

  // Allocate new packet for the output.
  AVPacket dest_packet;
  if (av_new_packet(&dest_packet, output_packet_size) != 0) {
    DVLOG(2) << __func__ << ": Memory allocation failure";
    return false;
  }

  // This is a bit tricky: since the interface does not allow us to replace
  // the pointer of the old packet with a new one, we will initially copy the
  // metadata from old packet to new bigger packet.
  av_packet_copy_props(&dest_packet, packet);

  // Proceed with the conversion of the actual in-band NAL units, leave room
  // for configuration in the beginning.
  uint32_t io_size = dest_packet.size;
  if (!converter_.ConvertNalUnitStreamToByteStream(
          packet->data, packet->size,
          avc_config.get(),
          dest_packet.data, &io_size)) {
    DVLOG(2) << __func__ << ": ConvertNalUnitStreamToByteStream() failure";
    return false;
  }

  // It is possible for the actual size to be smaller than the computed
  // allocation size.
  dest_packet.size = io_size;

  if (avc_config)
    configuration_processed_ = true;

  // At the end we must destroy the old packet.
  av_packet_unref(packet);

  // Finally, replace the values in the input packet.
  memcpy(packet, &dest_packet, sizeof(*packet));
  return true;
}

}  // namespace media
