// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/media_recorder.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/browser/devtools/devtools_stream_file.h"
#include "content/browser/media/capture/web_contents_video_capture_device.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "media/audio/audio_input_device.h"
#include "media/audio/audio_input_ipc.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/media_buildflags.h"
#include "media/mojo/common/input_error_code_converter.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"

namespace content::protocol {

namespace {

class DummyObserver final : public media::mojom::AudioInputStreamObserver {
 public:
  void DidStartRecording() final {}
};

// Implements the AudioInputIPC and AudioInputStreamClient interfaces to capture
// loopback audio for the media recorder.
class LoopbackAudioInputIPC : public media::AudioInputIPC,
                              public media::mojom::AudioInputStreamClient {
 public:
  explicit LoopbackAudioInputIPC(const base::UnguessableToken& group_id)
      : group_id_(group_id) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~LoopbackAudioInputIPC() override = default;

  // media::AudioInputIPC
  void CreateStream(media::AudioInputIPCDelegate* delegate,
                    const media::AudioParameters& params,
                    bool automatic_gain_control,
                    uint32_t total_segments) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    delegate_ = delegate;

    content::GetAudioService().BindStreamFactory(
        stream_factory_.BindNewPipeAndPassReceiver());

    auto client = stream_client_receiver_.BindNewPipeAndPassRemote();
    stream_client_receiver_.set_disconnect_handler(base::BindOnce(
        &LoopbackAudioInputIPC::OnError, weak_factory_.GetWeakPtr(),
        media::mojom::InputStreamErrorCode::kUnknown));

    mojo::PendingRemote<media::mojom::AudioInputStreamObserver> observer;
    mojo::MakeSelfOwnedReceiver(std::make_unique<DummyObserver>(),
                                observer.InitWithNewPipeAndPassReceiver());

    stream_factory_->CreateLoopbackStream(
        stream_.BindNewPipeAndPassReceiver(), std::move(client),
        std::move(observer), params, total_segments, group_id_,
        base::BindOnce(&LoopbackAudioInputIPC::StreamCreated,
                       weak_factory_.GetWeakPtr()));
  }

  void RecordStream() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (stream_.is_bound()) {
      stream_->Record();
    }
  }

  void SetVolume(double volume) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (stream_.is_bound()) {
      stream_->SetVolume(volume);
    }
  }

  void SetOutputDeviceForAec(const std::string& output_device_id) override {}

  void CloseStream() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    delegate_ = nullptr;
    stream_client_receiver_.reset();
    stream_.reset();
    weak_factory_.InvalidateWeakPtrs();
  }

  // media::mojom::AudioInputStreamClient
  void OnError(media::mojom::InputStreamErrorCode code) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (delegate_) {
      delegate_->OnError(media::ConvertToCaptureCallbackCode(code));
    }
  }

  void OnMutedStateChanged(bool is_muted) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (delegate_) {
      delegate_->OnMuted(is_muted);
    }
  }

  void StreamCreated(media::mojom::ReadWriteAudioDataPipePtr data_pipe) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (data_pipe.is_null() || !delegate_) {
      OnError(media::mojom::InputStreamErrorCode::kUnknown);
      return;
    }
    base::ScopedPlatformFile socket_handle =
        data_pipe->socket.TakePlatformFile();
    delegate_->OnStreamCreated(std::move(data_pipe->shared_memory),
                               std::move(socket_handle), false);
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::UnguessableToken group_id_;
  raw_ptr<media::AudioInputIPCDelegate> delegate_ = nullptr;
  mojo::Remote<media::mojom::AudioStreamFactory> stream_factory_;
  mojo::Remote<media::mojom::AudioInputStream> stream_;
  mojo::Receiver<media::mojom::AudioInputStreamClient> stream_client_receiver_{
      this};
  base::WeakPtrFactory<LoopbackAudioInputIPC> weak_factory_{this};
};

class MediaRecorderVideoFrameReceiver : public media::VideoFrameReceiver {
 public:
  explicit MediaRecorderVideoFrameReceiver(
      base::WeakPtr<MediaRecorder> recorder)
      : recorder_(std::move(recorder)) {}
  ~MediaRecorderVideoFrameReceiver() override = default;

  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    if (recorder_) {
      recorder_->OnNewBuffer(buffer_id, std::move(buffer_handle));
    }
  }

  void OnFrameReadyInBuffer(media::ReadyFrameInBuffer frame) override {
    if (recorder_) {
      recorder_->OnFrameReadyInBuffer(std::move(frame));
    }
  }

  void OnBufferRetired(int buffer_id) override {
    if (recorder_) {
      recorder_->OnBufferRetired(buffer_id);
    }
  }

  void OnCaptureConfigurationChanged() override {}
  void OnError(media::VideoCaptureError error) override {}
  void OnFrameDropped(media::VideoCaptureFrameDropReason reason) override {}
  void OnNewCaptureVersion(media::CaptureVersion capture_version) override {}
  void OnFrameWithEmptyRegionCapture() override {}
  void OnLog(const std::string& message) override {}
  void OnStarted() override {}
  void OnStartedUsingGpuDecode() override {}
  void OnStopped() override {}

 private:
  base::WeakPtr<MediaRecorder> recorder_;
};

#if BUILDFLAG(ENABLE_LIBAOM)
// Determines the snapshot size that best-fits the Surface's content to the
// remote's requested image size.
gfx::Size DetermineSnapshotSize(const gfx::Size& surface_size,
                                int screencast_max_width,
                                int screencast_max_height) {
  if (surface_size.IsEmpty()) {
    return gfx::Size();  // Nothing to copy (and avoid divide-by-zero below).
  }

  double scale = 1;
  if (screencast_max_width > 0) {
    scale = std::min(scale, static_cast<double>(screencast_max_width) /
                                surface_size.width());
  }
  if (screencast_max_height > 0) {
    scale = std::min(scale, static_cast<double>(screencast_max_height) /
                                surface_size.height());
  }
  return gfx::ToRoundedSize(gfx::ScaleSize(gfx::SizeF(surface_size), scale));
}
#endif  // BUILDFLAG(ENABLE_LIBAOM)
}  // namespace

MediaRecorder::ClientBuffer::ClientBuffer() = default;
MediaRecorder::ClientBuffer::ClientBuffer(
    base::ReadOnlySharedMemoryMapping read_only_mapping,
    base::WritableSharedMemoryMapping writable_mapping,
    media::mojom::VideoBufferHandlePtr buffer_handle)
    : read_only_mapping(std::move(read_only_mapping)),
      writable_mapping(std::move(writable_mapping)),
      buffer_handle(std::move(buffer_handle)) {}
MediaRecorder::ClientBuffer::~ClientBuffer() = default;

MediaRecorder::MediaRecorder(DevToolsIOContext* io_context,
                             base::RepeatingClosure on_stop_recording)
    : io_context_(io_context),
      on_stop_recording_(std::move(on_stop_recording)) {}

MediaRecorder::~MediaRecorder() {
  Stop(base::OnceCallback<void(std::string)>());
}

Response MediaRecorder::Start(RenderFrameHostImpl* host,
                              bool audio,
                              int max_width,
                              int max_height,
                              int frame_rate) {
#if !BUILDFLAG(ENABLE_LIBAOM)
  return Response::ServerError(
      "Video recording is not supported without AV1/libaom enabled.");
#else
  session_start_time_ = base::TimeTicks::Now();

  max_width_ = max_width;
  max_height_ = max_height;

  gfx::Size surface_size = gfx::Size();
  RenderWidgetHostViewBase* const view =
      static_cast<RenderWidgetHostViewBase*>(host->GetView());
  if (view) {
    surface_size = view->GetCompositorViewportPixelSize();
    last_surface_size_ = surface_size;
  }

  stream_file_ = DevToolsStreamFile::Create(io_context_, true /* binary */);
  stream_ = stream_file_->handle();

  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      remote = content::ServiceProcessHost::Launch<
          devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>(
          ServiceProcessHost::Options()
              .WithDisplayName("DevTools Media Encoding Service")
#if BUILDFLAG(IS_MAC)
              .WithChildFlags(ChildProcessHost::CHILD_RENDERER)
#endif
              .Pass());

  auto* web_contents = WebContentsImpl::FromRenderFrameHostImpl(host);
  bool has_audio = audio && web_contents != nullptr;

  remote->StartRecording(client_receiver_.BindNewPipeAndPassRemote(), max_width,
                         max_height, frame_rate, has_audio);
  client_receiver_.set_disconnect_handler(
      base::BindOnce(&MediaRecorder::OnClosed, base::Unretained(this)));
  service_ = mojo::SharedRemote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>(
      remote.Unbind());

  gfx::Size snapshot_size =
      DetermineSnapshotSize(surface_size, max_width_, max_height_);
  if (!snapshot_size.IsEmpty()) {
    last_surface_size_ = snapshot_size;
  }

#if BUILDFLAG(IS_ANDROID)
  constexpr auto kScreencastPixelFormat = media::PIXEL_FORMAT_I420;
#else
  constexpr auto kScreencastPixelFormat = media::PIXEL_FORMAT_ARGB;
#endif

  constexpr int kIdleKeepAliveIntervalMs = 1000;

  content::WebContentsMediaCaptureId capture_id(
      host->GetProcess()->GetDeprecatedID(), host->GetRoutingID());
  video_capturer_ =
      WebContentsVideoCaptureDevice::Create(capture_id.ToString());
  if (video_capturer_) {
    video_capturer_->SetBufferFormatPreference(
        viz::mojom::BufferFormatPreference::kDefault);
  }

  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      snapshot_size.IsEmpty() ? gfx::Size(max_width > 0 ? max_width : 800,
                                          max_height > 0 ? max_height : 600)
                              : snapshot_size,
      frame_rate, kScreencastPixelFormat);

  if (video_capturer_) {
    video_capturer_->AllocateAndStartWithReceiver(
        params, std::make_unique<MediaRecorderVideoFrameReceiver>(
                    weak_factory_.GetWeakPtr()));
  }

  video_timer_ = std::make_unique<base::RepeatingTimer>();
  video_timer_->Start(FROM_HERE, base::Milliseconds(kIdleKeepAliveIntervalMs),
                      base::BindRepeating(&MediaRecorder::RequestRefreshFrame,
                                          weak_factory_.GetWeakPtr()));

  if (has_audio) {
    audio_capturer_ = base::MakeRefCounted<media::AudioInputDevice>(
        std::make_unique<LoopbackAudioInputIPC>(
            web_contents->GetAudioGroupId()),
        media::AudioInputDevice::Purpose::kLoopback,
        media::AudioInputDevice::DeadStreamDetection::kDisabled);

    media::AudioParameters audio_params(
        media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
        media::ChannelLayoutConfig::Stereo(), 48000, 480);
    audio_capturer_->Initialize(audio_params, this);
    audio_capturer_->Start();
  }

  return Response::Success();
#endif
}

void MediaRecorder::RequestRefreshFrame() {
  constexpr int kIdleKeepAliveIntervalMs = 1000;
  if (video_capturer_ && (last_frame_receive_time_.is_null() ||
                          base::TimeTicks::Now() - last_frame_receive_time_ >=
                              base::Milliseconds(kIdleKeepAliveIntervalMs))) {
    video_capturer_->RequestRefreshFrame();
  }
}

void MediaRecorder::OnNewBuffer(
    int32_t buffer_id,
    media::mojom::VideoBufferHandlePtr buffer_handle) {
  base::ReadOnlySharedMemoryMapping read_only_mapping;
  base::WritableSharedMemoryMapping writable_mapping;
  if (buffer_handle->is_read_only_shmem_region()) {
    read_only_mapping = buffer_handle->get_read_only_shmem_region().Map();
    if (!read_only_mapping.IsValid()) {
      DLOG(ERROR) << "Shared memory mapping failed.";
      return;
    }
  } else if (buffer_handle->is_unsafe_shmem_region()) {
    writable_mapping = buffer_handle->get_unsafe_shmem_region().Map();
    if (!writable_mapping.IsValid()) {
      DLOG(ERROR) << "Shared memory mapping failed.";
      return;
    }
  } else if (buffer_handle->is_gpu_memory_buffer_handle()) {
#if !BUILDFLAG(IS_MAC)
    DLOG(ERROR) << "Unsupported GMB VideoBufferHandle";
    return;
#endif
  } else {
    DLOG(ERROR) << "Unsupported VideoBufferHandle";
    return;
  }
  client_buffers_[buffer_id] = base::MakeRefCounted<ClientBuffer>(
      std::move(read_only_mapping), std::move(writable_mapping),
      std::move(buffer_handle));
}

void MediaRecorder::OnFrameReadyInBuffer(media::ReadyFrameInBuffer frame) {
  auto it = client_buffers_.find(frame.buffer_id);
  if (it == client_buffers_.end()) {
    return;
  }

  const auto& info = frame.frame_info;
  gfx::Rect visible_rect = info->visible_rect;
  scoped_refptr<media::VideoFrame> video_frame;

  if (it->second->read_only_mapping.IsValid()) {
    base::span<const uint8_t> mapping_memory =
        it->second->read_only_mapping.GetMemoryAsSpan<const uint8_t>();
    if (mapping_memory.size() < media::VideoFrame::AllocationSize(
                                    info->pixel_format, info->coded_size)) {
      DLOG(ERROR) << "Shared memory size was less than expected.";
      return;
    }
    video_frame = media::VideoFrame::WrapExternalData(
        info->pixel_format, info->coded_size, visible_rect, visible_rect.size(),
        mapping_memory, info->timestamp);
  } else if (it->second->writable_mapping.IsValid()) {
    base::span<const uint8_t> mapping_memory =
        it->second->writable_mapping.GetMemoryAsSpan<const uint8_t>();
    if (mapping_memory.size() < media::VideoFrame::AllocationSize(
                                    info->pixel_format, info->coded_size)) {
      DLOG(ERROR) << "Shared memory size was less than expected.";
      return;
    }
    video_frame = media::VideoFrame::WrapExternalData(
        info->pixel_format, info->coded_size, visible_rect, visible_rect.size(),
        mapping_memory, info->timestamp);
  } else if (it->second->buffer_handle &&
             it->second->buffer_handle->is_gpu_memory_buffer_handle()) {
#if BUILDFLAG(IS_MAC)
    video_frame = media::VideoFrame::WrapUnacceleratedIOSurface(
        it->second->buffer_handle->get_gpu_memory_buffer_handle().Clone(),
        visible_rect, visible_rect.size(), info->timestamp);
#endif
  }

  if (!video_frame) {
    DLOG(ERROR) << "Unable to create VideoFrame wrapper.";
    return;
  }

  video_frame->AddDestructionObserver(base::BindOnce(
      [](std::unique_ptr<
             media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
             permission,
         scoped_refptr<ClientBuffer> buffer) {},
      std::move(frame.buffer_read_permission), it->second));

  video_frame->set_metadata(info->metadata);
  video_frame->set_color_space(info->color_space);

  OnFrameFromVideoConsumer(std::move(video_frame));
}

void MediaRecorder::OnBufferRetired(int buffer_id) {
  client_buffers_.erase(buffer_id);
}

void MediaRecorder::OnFrameFromVideoConsumer(
    scoped_refptr<media::VideoFrame> frame) {
  if (!service_) {
    return;
  }

  has_received_frames_ = true;
  last_frame_receive_time_ = base::TimeTicks::Now();
  if (frame->timestamp().is_min() || frame->timestamp().is_zero()) {
    frame->set_timestamp(last_frame_receive_time_ - session_start_time_);
  }
  service_->RecordVideoFrame(std::move(frame));
}

void MediaRecorder::Capture(const media::AudioBus* audio_source,
                            base::TimeTicks audio_capture_time,
                            const media::AudioGlitchInfo& glitch_info,
                            double volume) {
  if (!service_ || !audio_source) {
    return;
  }

  base::TimeDelta timestamp = audio_capture_time - session_start_time_;
  auto buffer = media::AudioBuffer::CopyFrom(48000, timestamp, audio_source);
  service_->RecordAudioBuffer(
      mojo::ConvertTo<media::mojom::AudioBufferPtr>(*buffer));
}

void MediaRecorder::OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                                   const std::string& message) {
  if (service_) {
    Stop(base::OnceCallback<void(std::string)>());
  }
}

void MediaRecorder::OnCaptureMuted(bool is_muted) {}

void MediaRecorder::Stop(
    base::OnceCallback<void(std::string)> on_stop_callback) {
  on_stop_callback_ = std::move(on_stop_callback);
  if (video_timer_) {
    video_timer_->Stop();
    video_timer_.reset();
  }
  if (video_capturer_) {
    video_capturer_->StopAndDeAllocate();
    video_capturer_.reset();
  }
  if (audio_capturer_) {
    audio_capturer_->Stop();
    audio_capturer_ = nullptr;
  }

  if (service_) {
    if (!has_received_frames_) {
      // Inject a blank frame if absolutely no frames were received, to ensure
      // the muxer creates a video track.
      gfx::Size fallback_size(max_width_ > 0 ? max_width_ : 800,
                              max_height_ > 0 ? max_height_ : 600);
      scoped_refptr<media::VideoFrame> fallback_frame =
          media::VideoFrame::CreateBlackFrame(fallback_size);
      if (fallback_frame) {
        fallback_frame->set_timestamp(base::TimeTicks::Now() -
                                      session_start_time_);
        service_->RecordVideoFrame(std::move(fallback_frame));
      }
    }
    service_->StopRecording();
  } else if (on_stop_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_stop_callback_), stream_));
  }
}

void MediaRecorder::OnData(const std::vector<uint8_t>& data) {
  if (stream_file_) {
    stream_file_->Append(std::make_unique<std::string>(
        reinterpret_cast<const char*>(data.data()), data.size()));
  }
}

void MediaRecorder::OnClosed() {
  service_.reset();
  client_receiver_.reset();
  if (on_stop_callback_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_stop_callback_), stream_));
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, on_stop_recording_);
  }
}

}  // namespace content::protocol
