// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_PROTO_UTILS_H_
#define MEDIA_REMOTING_PROTO_UTILS_H_

#include <cstdint>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace media {
namespace remoting {

// Utility class to convert data between media::DecoderBuffer and byte array.
// It is to serialize media::DecoderBuffer structure except for actual data
// into openscreen::cast::DecoderBuffer followed by byte array of decoder
// buffer. The reason data is not part of proto buffer because it would cost
// unnecessary time to wait for whole proto received before conversion given the
// fact that decoder buffer data can vary from hundred bytes to 3~5MB. Also, it
// would costs extra CPU to serialize/de-serialize decoder buffer which is
// encoded and encrypted as wire format for data transmission.
//
// DecoderBufferSegment {
//  // Payload version. Default value is 0.
//  u8 payload_version;
//
//  // Length of openscreen::cast::DecoderBuffer (protobuf-encoded of
//  media::DecoderBuffer
//                   except for data).
//  u16 buffer_segment_size;
//  // openscreen::cast::DecoderBuffer.
//  u8[buffer_segment_size] buffer_segment;
//
//  // Length of data in media::DecoderBuffer.
//  u32 data_buffer_size;
//  // media::DecoderBuffer data.
//  u8[data_buffer_size] data_buffer;
//};

// Converts DecoderBufferSegment into byte array.
std::vector<uint8_t> DecoderBufferToByteArray(
    const DecoderBuffer& decoder_buffer);

// Converts byte array into DecoderBufferSegment.
scoped_refptr<DecoderBuffer> ByteArrayToDecoderBuffer(const uint8_t* data,
                                                      uint32_t size);

// Data type conversion between media::AudioDecoderConfig and proto buffer.
void ConvertAudioDecoderConfigToProto(
    const AudioDecoderConfig& audio_config,
    openscreen::cast::AudioDecoderConfig* audio_message);
bool ConvertProtoToAudioDecoderConfig(
    const openscreen::cast::AudioDecoderConfig& audio_message,
    AudioDecoderConfig* audio_config);

// Data type conversion between media::VideoDecoderConfig and proto buffer.
void ConvertVideoDecoderConfigToProto(
    const VideoDecoderConfig& video_config,
    openscreen::cast::VideoDecoderConfig* video_message);
bool ConvertProtoToVideoDecoderConfig(
    const openscreen::cast::VideoDecoderConfig& video_message,
    VideoDecoderConfig* video_config);

// Data type conversion between media::VideoDecoderConfig and proto buffer.
void ConvertProtoToPipelineStatistics(
    const openscreen::cast::PipelineStatistics& stats_message,
    PipelineStatistics* stats);

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_PROTO_UTILS_H_
