// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_PROTO_ENUM_UTILS_H_
#define MEDIA_REMOTING_PROTO_ENUM_UTILS_H_

#include "base/optional.h"
#include "media/base/audio_codecs.h"
#include "media/base/buffering_state.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace media {
namespace remoting {

// The following functions map between the enum values in media/base modules and
// the equivalents in the media/remoting protobuf classes. The purpose of these
// converters is to decouple the media/base modules from the media/remoting
// modules while maintaining compile-time checks to ensure that there are always
// valid, backwards-compatible mappings between the two.
//
// Each returns a base::Optional value. If it is not set, that indicates the
// conversion failed.

base::Optional<AudioCodec> ToMediaAudioCodec(
    openscreen::cast::AudioDecoderConfig::Codec value);
base::Optional<openscreen::cast::AudioDecoderConfig::Codec>
ToProtoAudioDecoderConfigCodec(AudioCodec value);

base::Optional<SampleFormat> ToMediaSampleFormat(
    openscreen::cast::AudioDecoderConfig::SampleFormat value);
base::Optional<openscreen::cast::AudioDecoderConfig::SampleFormat>
ToProtoAudioDecoderConfigSampleFormat(SampleFormat value);

base::Optional<ChannelLayout> ToMediaChannelLayout(
    openscreen::cast::AudioDecoderConfig::ChannelLayout value);
base::Optional<openscreen::cast::AudioDecoderConfig::ChannelLayout>
ToProtoAudioDecoderConfigChannelLayout(ChannelLayout value);

base::Optional<VideoCodec> ToMediaVideoCodec(
    openscreen::cast::VideoDecoderConfig::Codec value);
base::Optional<openscreen::cast::VideoDecoderConfig::Codec>
ToProtoVideoDecoderConfigCodec(VideoCodec value);

base::Optional<VideoCodecProfile> ToMediaVideoCodecProfile(
    openscreen::cast::VideoDecoderConfig::Profile value);
base::Optional<openscreen::cast::VideoDecoderConfig::Profile>
ToProtoVideoDecoderConfigProfile(VideoCodecProfile value);

base::Optional<VideoPixelFormat> ToMediaVideoPixelFormat(
    openscreen::cast::VideoDecoderConfig::Format value);

base::Optional<BufferingState> ToMediaBufferingState(
    openscreen::cast::RendererClientOnBufferingStateChange::State value);
base::Optional<openscreen::cast::RendererClientOnBufferingStateChange::State>
ToProtoMediaBufferingState(BufferingState value);

base::Optional<DemuxerStream::Status> ToDemuxerStreamStatus(
    openscreen::cast::DemuxerStreamReadUntilCallback::Status value);
base::Optional<openscreen::cast::DemuxerStreamReadUntilCallback::Status>
ToProtoDemuxerStreamStatus(DemuxerStream::Status value);

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_PROTO_ENUM_UTILS_H_
