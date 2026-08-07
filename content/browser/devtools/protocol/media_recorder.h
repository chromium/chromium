// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEDIA_RECORDER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEDIA_RECORDER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_stream_file.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/common/content_export.h"
#include "content/services/devtools_media_encoding_service/public/mojom/devtools_media_encoding_service.mojom.h"
#include "media/base/audio_capturer_source.h"
#include "media/capture/video/video_frame_receiver_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class RepeatingTimer;
class SequencedTaskRunner;
}  // namespace base

namespace content {
class DevToolsIOContext;
class InProcessLaunchedVideoCaptureDevice;
class RenderFrameHostImpl;
class WebContentsVideoCaptureDevice;

namespace protocol {

class CONTENT_EXPORT MediaRecorder
    : public media::AudioCapturerSource::CaptureCallback,
      public devtools_media_encoding_service::mojom::
          DevToolsMediaEncodingServiceClient {
 public:
  MediaRecorder(DevToolsIOContext* io_context,
                base::RepeatingClosure on_stop_recording);
  ~MediaRecorder() override;

  Response Start(RenderFrameHostImpl* host,
                 bool audio,
                 int max_width,
                 int max_height,
                 int frame_rate);

  void Stop(base::OnceCallback<void(std::string)> on_stop_callback);
  const std::string& GetStream() const { return stream_; }

  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle);
  void OnFrameReadyInBuffer(media::ReadyFrameInBuffer frame);
  void OnBufferRetired(int buffer_id);

 private:
  void OnFrameFromVideoConsumer(scoped_refptr<media::VideoFrame> frame);
  void RequestRefreshFrame();

  // media::AudioCapturerSource::CaptureCallback overrides
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               const media::AudioGlitchInfo& glitch_info,
               double volume) override;
  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) override;
  void OnCaptureMuted(bool is_muted) override;

  // devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient:
  void OnData(const std::vector<uint8_t>& data) override;
  void OnClosed() override;

  raw_ptr<DevToolsIOContext> io_context_;
  base::RepeatingClosure on_stop_recording_;

  int max_width_ = -1;
  int max_height_ = -1;
  bool has_received_frames_ = false;
  base::TimeTicks session_start_time_;
  base::TimeTicks last_frame_receive_time_;
  gfx::Size last_surface_size_;

  std::unique_ptr<WebContentsVideoCaptureDevice> video_capturer_;
  std::unique_ptr<base::RepeatingTimer> video_timer_;
  struct ClientBuffer : public base::RefCountedThreadSafe<ClientBuffer> {
    ClientBuffer();
    ClientBuffer(base::ReadOnlySharedMemoryMapping read_only_mapping,
                 base::WritableSharedMemoryMapping writable_mapping,
                 media::mojom::VideoBufferHandlePtr buffer_handle);
    base::ReadOnlySharedMemoryMapping read_only_mapping;
    base::WritableSharedMemoryMapping writable_mapping;
    media::mojom::VideoBufferHandlePtr buffer_handle;

   private:
    friend class base::RefCountedThreadSafe<ClientBuffer>;
    ~ClientBuffer();
  };
  base::flat_map<int, scoped_refptr<ClientBuffer>> client_buffers_;
  scoped_refptr<media::AudioCapturerSource> audio_capturer_;

  scoped_refptr<DevToolsStreamFile> stream_file_;
  base::OnceCallback<void(std::string)> on_stop_callback_;
  std::string stream_;

  mojo::SharedRemote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      service_;
  mojo::Receiver<devtools_media_encoding_service::mojom::
                     DevToolsMediaEncodingServiceClient>
      client_receiver_{this};

  base::WeakPtrFactory<MediaRecorder> weak_factory_{this};
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEDIA_RECORDER_H_
