// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_DEVTOOLS_MEDIA_ENCODING_SERVICE_DEVTOOLS_MEDIA_ENCODING_SERVICE_IMPL_H_
#define CONTENT_SERVICES_DEVTOOLS_MEDIA_ENCODING_SERVICE_DEVTOOLS_MEDIA_ENCODING_SERVICE_IMPL_H_

#include <deque>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/services/devtools_media_encoding_service/public/mojom/devtools_media_encoding_service.mojom.h"
#include "media/base/audio_encoder.h"
#include "media/base/encoder_status.h"
#include "media/base/video_encoder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

namespace media {
class AudioBuffer;
class MuxerTimestampAdapter;
}  // namespace media

namespace content {

class CONTENT_EXPORT DevToolsMediaEncodingServiceImpl
    : public devtools_media_encoding_service::mojom::
          DevToolsMediaEncodingService {
 public:
  explicit DevToolsMediaEncodingServiceImpl(
      mojo::PendingReceiver<
          devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
          receiver);
  DevToolsMediaEncodingServiceImpl(const DevToolsMediaEncodingServiceImpl&) =
      delete;
  DevToolsMediaEncodingServiceImpl& operator=(
      const DevToolsMediaEncodingServiceImpl&) = delete;
  ~DevToolsMediaEncodingServiceImpl() override;

  // devtools_media_encoding_service::mojom::DevToolsMediaEncodingService:
  void StartRecording(
      mojo::PendingRemote<devtools_media_encoding_service::mojom::
                              DevToolsMediaEncodingServiceClient> client,
      uint32_t max_width,
      uint32_t max_height,
      uint32_t frame_rate,
      bool has_audio) override;
  void RecordVideoFrame(const scoped_refptr<media::VideoFrame>& frame) override;
  void RecordAudioBuffer(media::mojom::AudioBufferPtr buffer) override;
  void StopRecording() override;

 private:
  void OnScreencastEncodedFrame(
      media::VideoEncoderOutput output,
      std::optional<media::VideoEncoder::CodecDescription> description);

  void OnScreencastEncodedAudio(
      media::EncodedAudioBuffer encoded_audio,
      std::optional<media::AudioEncoder::CodecDescription> description);

  void OnEncoderFlushed(media::EncoderStatus status);
  void OnVideoEncoderFlushedAndRecreate(media::EncoderStatus status);

  void ProcessVideoFrameQueue();
  void TryFlushEncoders();

  mojo::Receiver<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      receiver_{this};
  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient>
      client_;

  uint32_t screencast_frame_rate_ = 0;
  bool has_audio_ = false;
  gfx::Size last_surface_size_;

  std::unique_ptr<media::VideoEncoder> screencast_video_encoder_;
  std::unique_ptr<media::AudioEncoder> screencast_audio_encoder_;
  std::unique_ptr<media::MuxerTimestampAdapter> screencast_mp4_muxer_;

  bool video_encoder_flushing_ = false;
  bool stopping_ = false;
  bool wait_for_queues_to_finish_ = false;

  std::deque<scoped_refptr<media::VideoFrame>> video_frame_queue_;

  scoped_refptr<media::VideoFrame> last_video_frame_;
  base::TimeTicks last_video_frame_receive_time_;

  int encoders_flushing_ = 0;

  base::WeakPtrFactory<DevToolsMediaEncodingServiceImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_SERVICES_DEVTOOLS_MEDIA_ENCODING_SERVICE_DEVTOOLS_MEDIA_ENCODING_SERVICE_IMPL_H_
