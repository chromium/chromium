// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_OPENSCREEN_REMOTING_MESSAGE_FACTORIES_H_
#define MEDIA_CAST_OPENSCREEN_REMOTING_MESSAGE_FACTORIES_H_

#include <memory>

#include "base/time/time.h"
#include "media/base/buffering_state.h"
#include "media/base/pipeline_status.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace media {
class AudioDecoderConfig;
class VideoDecoderConfig;
}  // namespace media

namespace openscreen {
namespace cast {
class RpcMessage;
}  // namespace cast
}  // namespace openscreen

namespace media::cast {

// Each of these methods creates an RpcMessage type representing the operation
// called out in the name. They are intended to be used by a media::Renderer to
// communicate using the Cast Mirroring protocol.
//
// Note that these utilities do NOT set the handle for the created messages, and
// the caller is instead expected to set this.
std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForError();

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForMediaEnded();

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForStatisticsUpdate(
    const media::PipelineStatistics& stats);

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForBufferingStateChange(media::BufferingState state);

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForAudioConfigChange(
    const media::AudioDecoderConfig& config);

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForVideoConfigChange(
    const media::VideoDecoderConfig& config);

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForVideoNaturalSizeChange(const gfx::Size& size);

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForVideoOpacityChange(bool opaque);

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForMediaTimeUpdate(
    base::TimeDelta media_time);

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForInitializationComplete(bool has_succeeded);

std::unique_ptr<openscreen::cast::RpcMessage> CreateMessageForFlushComplete();

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForAcquireRendererDone(
    openscreen::cast::RpcMessenger::Handle receiver_renderer_handle);

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForDemuxerStreamInitialize(
    openscreen::cast::RpcMessenger::Handle local_handle);

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForDemuxerStreamReadUntil(
    openscreen::cast::RpcMessenger::Handle local_handle,
    uint32_t buffers_requested);

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForDemuxerStreamEnableBitstreamConverter();

std::unique_ptr<openscreen::cast::RpcMessage>
CreateMessageForDemuxerStreamError();

}  // namespace media::cast

#endif  // MEDIA_CAST_OPENSCREEN_REMOTING_MESSAGE_FACTORIES_H_
