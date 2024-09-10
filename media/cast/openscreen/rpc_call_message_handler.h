// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_OPENSCREEN_RPC_CALL_MESSAGE_HANDLER_H_
#define MEDIA_CAST_OPENSCREEN_RPC_CALL_MESSAGE_HANDLER_H_

#include <optional>

#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder_config.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

namespace openscreen::cast {
class RpcMessage;
}  // namespace openscreen::cast

namespace media::cast {

// This class is responsible for translating between
// openscreen::cast::RpcMessage commands (used by the remoting protocol) and
// chromium types that are more easily usable.
class RpcInitializationCallMessageHandler {
 public:
  virtual ~RpcInitializationCallMessageHandler();

  virtual void OnRpcAcquireRenderer(
      openscreen::cast::RpcMessenger::Handle handle) = 0;
  virtual void OnRpcAcquireDemuxer(
      openscreen::cast::RpcMessenger::Handle audio_stream_handle,
      openscreen::cast::RpcMessenger::Handle video_stream_handle) = 0;
};

class RpcRendererCallMessageHandler {
 public:
  virtual ~RpcRendererCallMessageHandler();

  virtual void OnRpcInitialize() = 0;
  virtual void OnRpcFlush(uint32_t audio_count, uint32_t video_count) = 0;
  virtual void OnRpcStartPlayingFrom(base::TimeDelta time) = 0;
  virtual void OnRpcSetPlaybackRate(double playback_rate) = 0;
  virtual void OnRpcSetVolume(double volume) = 0;
};

class RpcDemuxerStreamCBMessageHandler {
 public:
  virtual ~RpcDemuxerStreamCBMessageHandler();

  virtual void OnRpcInitializeCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      std::optional<media::AudioDecoderConfig> audio_config,
      std::optional<media::VideoDecoderConfig> video_config) = 0;
  virtual void OnRpcReadUntilCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      std::optional<media::AudioDecoderConfig> audio_config,
      std::optional<media::VideoDecoderConfig> video_config,
      uint32_t total_frames_received) = 0;
  virtual void OnRpcEnableBitstreamConverterCallback(
      openscreen::cast::RpcMessenger::Handle handle,
      bool succeeded) = 0;
};

// Processes the incoming |message| and forwards it to the appropriate |client|
// method.
bool DispatchInitializationRpcCall(openscreen::cast::RpcMessage* message,
                                   RpcInitializationCallMessageHandler* client);
bool DispatchRendererRpcCall(openscreen::cast::RpcMessage* message,
                             RpcRendererCallMessageHandler* client);
bool DispatchDemuxerStreamCBRpcCall(openscreen::cast::RpcMessage* message,
                                    RpcDemuxerStreamCBMessageHandler* client);

}  // namespace media::cast

#endif  // MEDIA_CAST_OPENSCREEN_RPC_CALL_MESSAGE_HANDLER_H_
