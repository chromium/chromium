// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_OPENSCREEN_REMOTING_PROTO_ENUM_UTILS_H_
#define MEDIA_CAST_OPENSCREEN_REMOTING_PROTO_ENUM_UTILS_H_

#include "media/base/audio_codecs.h"
#include "media/base/buffering_state.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace media::cast {

// The following functions map between the enum values in media/base modules and
// the equivalents in the media/remoting protobuf classes. The purpose of these
// converters is to decouple the media/base modules from the media/remoting
// modules while maintaining compile-time checks to ensure that there are always
// valid, backwards-compatible mappings between the two.
//
// Each returns a absl::optional value. If it is not set, that indicates the
// conversion failed.

absl::optional<media::AudioCodec> ToMediaAudioCodec(
    openscreen::cast::AudioDecoderConfig::Codec value);
absl::optional<openscreen::cast::AudioDecoderConfig::Codec>
ToProtoAudioDecoderConfigCodec(media::AudioCodec value);

absl::optional<media::SampleFormat> ToMediaSampleFormat(
    openscreen::cast::AudioDecoderConfig::SampleFormat value);
absl::optional<openscreen::cast::AudioDecoderConfig::SampleFormat>
ToProtoAudioDecoderConfigSampleFormat(media::SampleFormat value);

absl::optional<media::ChannelLayout> ToMediaChannelLayout(
    openscreen::cast::AudioDecoderConfig::ChannelLayout value);
absl::optional<openscreen::cast::AudioDecoderConfig::ChannelLayout>
ToProtoAudioDecoderConfigChannelLayout(media::ChannelLayout value);

absl::optional<media::VideoCodec> ToMediaVideoCodec(
    openscreen::cast::VideoDecoderConfig::Codec value);
absl::optional<openscreen::cast::VideoDecoderConfig::Codec>
ToProtoVideoDecoderConfigCodec(media::VideoCodec value);

absl::optional<media::VideoCodecProfile> ToMediaVideoCodecProfile(
    openscreen::cast::VideoDecoderConfig::Profile value);
absl::optional<openscreen::cast::VideoDecoderConfig::Profile>
ToProtoVideoDecoderConfigProfile(media::VideoCodecProfile value);

absl::optional<media::VideoPixelFormat> ToMediaVideoPixelFormat(
    openscreen::cast::VideoDecoderConfig::Format value);

absl::optional<media::BufferingState> ToMediaBufferingState(
    openscreen::cast::RendererClientOnBufferingStateChange::State value);
absl::optional<openscreen::cast::RendererClientOnBufferingStateChange::State>
ToProtoMediaBufferingState(media::BufferingState value);

absl::optional<media::DemuxerStream::Status> ToDemuxerStreamStatus(
    openscreen::cast::DemuxerStreamReadUntilCallback::Status value);
absl::optional<openscreen::cast::DemuxerStreamReadUntilCallback::Status>
ToProtoDemuxerStreamStatus(media::DemuxerStream::Status value);

}  // namespace media::cast

#endif  // MEDIA_CAST_OPENSCREEN_REMOTING_PROTO_ENUM_UTILS_H_
