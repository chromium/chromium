// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_STREAM_PROVIDER_H_
#define MEDIA_REMOTING_STREAM_PROVIDER_H_

#include "base/memory/weak_ptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_resource.h"
#include "media/base/video_decoder_config.h"
#include "media/remoting/rpc_broker.h"

namespace media {
namespace remoting {

class MediaStream;

// The media stream provider for Media Remoting receiver.
class StreamProvider final : public MediaResource {
 public:
  StreamProvider(RpcBroker* rpc_broker, base::OnceClosure error_callback);

  ~StreamProvider() override;

  // MediaResource implemenation.
  std::vector<DemuxerStream*> GetAllStreams() override;

  void Initialize(int remote_audio_handle,
                  int remote_video_handle,
                  base::OnceClosure callback);
  void AppendBuffer(DemuxerStream::Type type,
                    scoped_refptr<DecoderBuffer> buffer);
  void FlushUntil(DemuxerStream::Type type, int count);

 private:
  // Called when audio/video stream is initialized.
  void AudioStreamInitialized();
  void VideoStreamInitialized();

  // Called when any error occurs.
  void OnError(const std::string& error);

  RpcBroker* const rpc_broker_;  // Outlives this class.
  std::unique_ptr<MediaStream> video_stream_;
  std::unique_ptr<MediaStream> audio_stream_;
  bool audio_stream_initialized_ = false;
  bool video_stream_initialized_ = false;

  // Set when Initialize() is called, and will run when both video and audio
  // streams are initialized or error occurs.
  base::OnceClosure init_done_callback_;

  // Run when first error occurs;
  base::OnceClosure error_callback_;

  base::WeakPtrFactory<StreamProvider> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StreamProvider);
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_STREAM_PROVIDER_H_
