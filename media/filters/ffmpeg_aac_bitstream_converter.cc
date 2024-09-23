// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/ffmpeg_aac_bitstream_converter.h"

#include "base/logging.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

namespace {

// Creates an ADTS header and stores in |hdr|
// Assumes |hdr| points to an array of length |kAdtsHeaderSize|
// Returns false if parameter values are for an unsupported configuration.
bool GenerateAdtsHeader(int codec,
                        int layer,
                        int audio_profile,
                        int sample_rate_index,
                        int private_stream,
                        int channel_configuration,
                        int originality,
                        int home,
                        int copyrighted_stream,
                        int copyright_start,
                        int frame_length,
                        int buffer_fullness,
                        int number_of_frames_minus_one,
                        uint8_t* hdr) {
  DCHECK_EQ(codec, AV_CODEC_ID_AAC);

  memset(reinterpret_cast<void *>(hdr), 0,
         FFmpegAACBitstreamConverter::kAdtsHeaderSize);
  // Ref: http://wiki.multimedia.cx/index.php?title=ADTS
  // ADTS header structure is the following
  // AAAAAAAA  AAAABCCD  EEFFFFGH  HHIJKLMM  MMMMMMMM  MMMOOOOO  OOOOOOPP
  //
  // A    Syncword 0xFFF, all bits must be 1
  // B    MPEG Version: 0 for MPEG-4, 1 for MPEG-2
  // C    Layer: always 0
  // D    Protection absent: Set to 1 if no CRC and 0 if there is CRC.
  // E    Profile: the MPEG-4 Audio Object Type minus 1.
  // F    MPEG-4 Sampling Frequency Index (15 is forbidden)
  // G    Private stream:
  // H    MPEG-4 Channel Configuration
  // I    Originality
  // J    Home
  // K    Copyrighted Stream
  // L    Copyright_ start
  // M    Frame length. This must include the ADTS header length.
  // O    Buffer fullness
  // P    Number of AAC frames in ADTS frame minus 1.
  //      For maximum compatibility always use 1 AAC frame per ADTS frame.

  // Syncword
  hdr[0] = 0xFF;
  hdr[1] = 0xF0;

  // Layer is always 0. No further action required.

  // Protection absent (no CRC) is always 1.
  hdr[1] |= 1;

  switch (audio_profile) {
    case FF_PROFILE_AAC_MAIN:
      break;
    case FF_PROFILE_AAC_HE:
    case FF_PROFILE_AAC_HE_V2:
    case FF_PROFILE_AAC_LOW:
      hdr[2] |= (1 << 6);
      break;
    case FF_PROFILE_AAC_SSR:
      hdr[2] |= (2 << 6);
      break;
    case FF_PROFILE_AAC_LTP:
      hdr[2] |= (3 << 6);
      break;
    default:
      DLOG(ERROR) << "[" << __func__ << "] "
                  << "unsupported audio profile:" << audio_profile;
      return false;
  }

  hdr[2] |= ((sample_rate_index & 0xf) << 2);

  if (private_stream)
    hdr[2] |= (1 << 1);

  switch (channel_configuration) {
    case 1:
      // front-center
      hdr[3] |= (1 << 6);
      break;
    case 2:
      // front-left, front-right
      hdr[3] |= (2 << 6);
      break;
    case 3:
      // front-center, front-left, front-right
      hdr[3] |= (3 << 6);
      break;
    case 4:
      // front-center, front-left, front-right, back-center
      hdr[2] |= 1;
      break;
    case 5:
      // front-center, front-left, front-right, back-left, back-right
      hdr[2] |= 1;
      hdr[3] |= (1 << 6);
      break;
    case 6:
      // front-center, front-left, front-right, back-left, back-right,
      // LFE-channel
      hdr[2] |= 1;
      hdr[3] |= (2 << 6);
      break;
    case 8:
      // front-center, front-left, front-right, side-left, side-right,
      // back-left, back-right, LFE-channel
      hdr[2] |= 1;
      hdr[3] |= (3 << 6);
      break;
    default:
      DLOG(ERROR) << "[" << __func__ << "] "
                  << "unsupported number of audio channels:"
                  << channel_configuration;
      return false;
  }

  if (originality)
    hdr[3] |= (1 << 5);

  if (home)
    hdr[3] |= (1 << 4);

  if (copyrighted_stream)
    hdr[3] |= (1 << 3);

  if (copyright_start)
    hdr[3] |= (1 << 2);

  // frame length
  hdr[3] |= (frame_length >> 11) & 0x03;
  hdr[4] = (frame_length >> 3) & 0xFF;
  hdr[5] |= (frame_length & 7) << 5;

  // buffer fullness
  hdr[5] |= (buffer_fullness >> 6) & 0x1F;
  hdr[6] |= (buffer_fullness & 0x3F) << 2;

  hdr[6] |= number_of_frames_minus_one & 0x3;

  return true;
}

}

FFmpegAACBitstreamConverter::FFmpegAACBitstreamConverter(
    AVCodecParameters* stream_codec_parameters)
    : stream_codec_parameters_(stream_codec_parameters),
      header_generated_(false),
      codec_(),
      audio_profile_(),
      sample_rate_index_(),
      channel_configuration_(),
      frame_length_() {
  CHECK(stream_codec_parameters_);
}

FFmpegAACBitstreamConverter::~FFmpegAACBitstreamConverter() = default;

bool FFmpegAACBitstreamConverter::ConvertPacket(AVPacket* packet) {
  if (packet == NULL || !packet->data) {
    return false;
  }

  int header_plus_packet_size =
      packet->size + kAdtsHeaderSize;
  if (!stream_codec_parameters_->extradata) {
    DLOG(ERROR) << "extradata is null";
    return false;
  }
  if (stream_codec_parameters_->extradata_size < 2) {
    DLOG(ERROR) << "extradata too small to contain MP4A header";
    return false;
  }
  int sample_rate_index =
      ((stream_codec_parameters_->extradata[0] & 0x07) << 1) |
      ((stream_codec_parameters_->extradata[1] & 0x80) >> 7);
  if (sample_rate_index > 12) {
    sample_rate_index = 4;
  }

  if (!header_generated_ || codec_ != stream_codec_parameters_->codec_id ||
      audio_profile_ != stream_codec_parameters_->profile ||
      sample_rate_index_ != sample_rate_index ||
      channel_configuration_ !=
          stream_codec_parameters_->ch_layout.nb_channels ||
      frame_length_ != header_plus_packet_size) {
    header_generated_ =
        GenerateAdtsHeader(stream_codec_parameters_->codec_id,
                           0,  // layer
                           stream_codec_parameters_->profile, sample_rate_index,
                           0,  // private stream
                           stream_codec_parameters_->ch_layout.nb_channels,
                           0,  // originality
                           0,  // home
                           0,  // copyrighted_stream
                           0,  // copyright_ start
                           header_plus_packet_size,
                           0x7FF,  // buffer fullness
                           0,      // one frame per packet
                           hdr_);
    codec_ = stream_codec_parameters_->codec_id;
    audio_profile_ = stream_codec_parameters_->profile;
    sample_rate_index_ = sample_rate_index;
    channel_configuration_ = stream_codec_parameters_->ch_layout.nb_channels;
    frame_length_ = header_plus_packet_size;
  }

  // Inform caller if the header generation failed.
  if (!header_generated_)
    return false;

  // Allocate new packet for the output.
  AVPacket dest_packet;
  if (av_new_packet(&dest_packet, header_plus_packet_size) != 0)
    return false;  // Memory allocation failure.

  memcpy(dest_packet.data, hdr_, kAdtsHeaderSize);
  memcpy(reinterpret_cast<void*>(dest_packet.data + kAdtsHeaderSize),
         reinterpret_cast<void*>(packet->data), packet->size);

  // This is a bit tricky: since the interface does not allow us to replace
  // the pointer of the old packet with a new one, we will initially copy the
  // metadata from old packet to new bigger packet.
  av_packet_copy_props(&dest_packet, packet);

  // Release the old packet.
  av_packet_unref(packet);

  // Finally, replace the values in the input packet.
  memcpy(packet, &dest_packet, sizeof(*packet));
  return true;
}

}  // namespace media
