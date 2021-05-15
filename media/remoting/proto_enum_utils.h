// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_PROTO_ENUM_UTILS_H_
#define MEDIA_REMOTING_PROTO_ENUM_UTILS_H_

#include "media/base/audio_codecs.h"
#include "media/base/buffering_state.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace media {
namespace remoting {

// The following functions map between the enum values in media/base modules and
// the equivalents in the media/remoting protobuf classes. The purpose of these
// converters is to decouple the media/base modules from the media/remoting
// modules while maintaining compile-time checks to ensure that there are always
// valid, backwards-compatible mappings between the two.
//
// Each returns a absl::optional value. If it is not set, that indicates the
// conversion failed.

absl::optional<AudioCodec> ToMediaAudioCodec(
    openscreen::cast::AudioDecoderConfig::Codec value);
absl::optional<openscreen::cast::AudioDecoderConfig::Codec>
ToProtoAudioDecoderConfigCodec(AudioCodec value);

absl::optional<SampleFormat> ToMediaSampleFormat(
    openscreen::cast::AudioDecoderConfig::SampleFormat value);
absl::optional<openscreen::cast::AudioDecoderConfig::SampleFormat>
ToProtoAudioDecoderConfigSampleFormat(SampleFormat value);

absl::optional<ChannelLayout> ToMediaChannelLayout(
    openscreen::cast::AudioDecoderConfig::ChannelLayout value);
absl::optional<openscreen::cast::AudioDecoderConfig::ChannelLayout>
ToProtoAudioDecoderConfigChannelLayout(ChannelLayout value);

absl::optional<VideoCodec> ToMediaVideoCodec(
    openscreen::cast::VideoDecoderConfig::Codec value);
absl::optional<openscreen::cast::VideoDecoderConfig::Codec>
ToProtoVideoDecoderConfigCodec(VideoCodec value);

absl::optional<VideoCodecProfile> ToMediaVideoCodecProfile(
    openscreen::cast::VideoDecoderConfig::Profile value);
absl::optional<openscreen::cast::VideoDecoderConfig::Profile>
ToProtoVideoDecoderConfigProfile(VideoCodecProfile value);

absl::optional<VideoPixelFormat> ToMediaVideoPixelFormat(
    openscreen::cast::VideoDecoderConfig::Format value);

absl::optional<BufferingState> ToMediaBufferingState(
    openscreen::cast::RendererClientOnBufferingStateChange::State value);
absl::optional<openscreen::cast::RendererClientOnBufferingStateChange::State>
ToProtoMediaBufferingState(BufferingState value);

absl::optional<DemuxerStream::Status> ToDemuxerStreamStatus(
    openscreen::cast::DemuxerStreamReadUntilCallback::Status value);
absl::optional<openscreen::cast::DemuxerStreamReadUntilCallback::Status>
ToProtoDemuxerStreamStatus(DemuxerStream::Status value);

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_PROTO_ENUM_UTILS_H_
