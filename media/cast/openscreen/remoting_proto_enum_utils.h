// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_OPENSCREEN_REMOTING_PROTO_ENUM_UTILS_H_
#define MEDIA_CAST_OPENSCREEN_REMOTING_PROTO_ENUM_UTILS_H_

#include <optional>

#include "media/base/audio_codecs.h"
#include "media/base/buffering_state.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace media::cast {

// The following functions map between the enum values in media/base modules and
// the equivalents in the media/remoting protobuf classes. The purpose of these
// converters is to decouple the media/base modules from the media/remoting
// modules while maintaining compile-time checks to ensure that there are always
// valid, backwards-compatible mappings between the two.
//
// Each returns a std::optional value. If it is not set, that indicates the
// conversion failed.

std::optional<media::AudioCodec> ToMediaAudioCodec(
    openscreen::cast::AudioDecoderConfig::Codec value);
std::optional<openscreen::cast::AudioDecoderConfig::Codec>
ToProtoAudioDecoderConfigCodec(media::AudioCodec value);

std::optional<media::SampleFormat> ToMediaSampleFormat(
    openscreen::cast::AudioDecoderConfig::SampleFormat value);
std::optional<openscreen::cast::AudioDecoderConfig::SampleFormat>
ToProtoAudioDecoderConfigSampleFormat(media::SampleFormat value);

std::optional<media::ChannelLayout> ToMediaChannelLayout(
    openscreen::cast::AudioDecoderConfig::ChannelLayout value);
std::optional<openscreen::cast::AudioDecoderConfig::ChannelLayout>
ToProtoAudioDecoderConfigChannelLayout(media::ChannelLayout value);

std::optional<media::VideoCodec> ToMediaVideoCodec(
    openscreen::cast::VideoDecoderConfig::Codec value);
std::optional<openscreen::cast::VideoDecoderConfig::Codec>
ToProtoVideoDecoderConfigCodec(media::VideoCodec value);

std::optional<media::VideoCodecProfile> ToMediaVideoCodecProfile(
    openscreen::cast::VideoDecoderConfig::Profile value);
std::optional<openscreen::cast::VideoDecoderConfig::Profile>
ToProtoVideoDecoderConfigProfile(media::VideoCodecProfile value);

std::optional<media::VideoPixelFormat> ToMediaVideoPixelFormat(
    openscreen::cast::VideoDecoderConfig::Format value);

std::optional<media::BufferingState> ToMediaBufferingState(
    openscreen::cast::RendererClientOnBufferingStateChange::State value);
std::optional<openscreen::cast::RendererClientOnBufferingStateChange::State>
ToProtoMediaBufferingState(media::BufferingState value);

std::optional<media::DemuxerStream::Status> ToDemuxerStreamStatus(
    openscreen::cast::DemuxerStreamReadUntilCallback::Status value);
std::optional<openscreen::cast::DemuxerStreamReadUntilCallback::Status>
ToProtoDemuxerStreamStatus(media::DemuxerStream::Status value);

}  // namespace media::cast

#endif  // MEDIA_CAST_OPENSCREEN_REMOTING_PROTO_ENUM_UTILS_H_
