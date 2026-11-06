// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEDIA_RECORDER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_MEDIA_RECORDER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
#include "base/timer/timer.h"
#include "content/browser/devtools/devtools_stream_file.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/common/content_export.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/services/devtools_media_encoding_service/public/mojom/devtools_media_encoding_service.mojom.h"
#include "media/base/audio_capturer_source.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video/video_frame_receiver_types.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-shared.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class RepeatingTimer;
class SequencedTaskRunner;
}  // namespace base

namespace content {
class DevToolsIOContext;
class InProcessLaunchedVideoCaptureDevice;
class MediaStreamUI;
class RenderFrameHostImpl;
class WebContents;
class WebContentsVideoCaptureDevice;

namespace protocol {

class DisplayCaptureLauncher;

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

  // Records a display surface, for example the browser window that shows the
  // target, with the device that getDisplayMedia() uses for `media_id`.
  // Captures audio from `audio_source` if `audio` is true. Starts `ui` (if not
  // null), which shows that the surface is captured, and owns it while the
  // capture runs. Runs `callback` after the first frame is captured, when the
  // UI pauses the capture, or after a timeout, or with an error, always in a
  // posted task. If the device fails after the first
  // frame, the capture stops and the UI is removed, but the recording stays
  // active until Stop(). Before the first frame, `callback` gets an error.
  void StartDisplayCapture(const DesktopMediaID& media_id,
                           WebContents* audio_source,
                           std::unique_ptr<MediaStreamUI> ui,
                           bool audio,
                           int max_width,
                           int max_height,
                           int frame_rate,
                           base::OnceCallback<void(Response)> callback);

  void Stop(base::OnceCallback<void(std::string)> on_stop_callback);
  const std::string& GetStream() const { return stream_; }

  base::WeakPtr<MediaRecorder> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle);
  void OnFrameReadyInBuffer(media::ReadyFrameInBuffer frame);
  void OnBufferRetired(int buffer_id);
  void OnVideoCaptureError();

 private:
  // Stops capturing frames and audio, and removes the UI. The recording stays
  // active until Stop().
  void StopDisplayCapture();
  // Answers the pending start, if any, with success.
  void AnswerPendingStart();
  // Answers the pending start, if any, with an error.
  void FailPendingStart(std::string_view message);
  // Pauses or resumes the capture when the UI asks for it, for example when a
  // data leak prevention policy restricts the captured content.
  void OnStateChangeFromUI(const DesktopMediaID& media_id,
                           blink::mojom::MediaStreamStateChange new_state);
  // Tells the UI that the device stopped, and then destroys the UI.
  void ReleaseUI();
  void StartEncodingService(int max_width,
                            int max_height,
                            int frame_rate,
                            bool has_audio);
  void StartAudioCapture(WebContents* audio_source);
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
  // Used instead of `video_capturer_` for the browser window.
  std::unique_ptr<DisplayCaptureLauncher> display_capture_launcher_;
  std::unique_ptr<media::VideoFrameReceiver> display_capture_receiver_;
  std::unique_ptr<MediaStreamUI> media_stream_ui_;
  // The label and the device of the capture that `media_stream_ui_` shows.
  std::string ui_label_;
  DesktopMediaID ui_media_id_;
  std::unique_ptr<base::RepeatingTimer> video_timer_;
  // Answers the pending start if the first frame does not arrive in time.
  base::OneShotTimer first_frame_timer_;
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
  base::OnceCallback<void(Response)> on_start_callback_;
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
