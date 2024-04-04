// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_media_to_mojo_adapter.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/scoped_async_trace.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_pool_util.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_metrics.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/video_capture/public/cpp/receiver_mojo_to_media_adapter.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/scoped_video_capture_jpeg_decoder.h"
#include "media/capture/video/chromeos/video_capture_jpeg_decoder_impl.h"
#elif BUILDFLAG(IS_WIN)
#include "media/capture/video/win/video_capture_device_factory_win.h"
#endif

namespace {

using ScopedCaptureTrace =
    media::TypedScopedAsyncTrace<media::TraceCategory::kVideoAndImageCapture>;

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<media::VideoCaptureJpegDecoder> CreateGpuJpegDecoder(
    scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
    media::MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_callback,
    media::VideoCaptureJpegDecoder::DecodeDoneCB decode_done_cb,
    base::RepeatingCallback<void(const std::string&)> send_log_message_cb) {
  return std::make_unique<media::ScopedVideoCaptureJpegDecoder>(
      std::make_unique<media::VideoCaptureJpegDecoderImpl>(
          jpeg_decoder_factory_callback, decoder_task_runner,
          std::move(decode_done_cb), std::move(send_log_message_cb)),
      decoder_task_runner);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void TakePhotoCallbackTrampoline(
    media::VideoCaptureDevice::TakePhotoCallback callback,
    std::unique_ptr<ScopedCaptureTrace> trace,
    media::mojom::BlobPtr blob) {
  std::move(callback).Run(std::move(blob));
}

}  // anonymous namespace

namespace video_capture {

#if BUILDFLAG(IS_CHROMEOS_ASH)
DeviceMediaToMojoAdapter::DeviceMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureDevice> device,
    media::MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_callback,
    scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner)
    : device_(std::move(device)),
      jpeg_decoder_factory_callback_(std::move(jpeg_decoder_factory_callback)),
      jpeg_decoder_task_runner_(std::move(jpeg_decoder_task_runner)) {}
#elif BUILDFLAG(IS_WIN)
DeviceMediaToMojoAdapter::DeviceMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureDevice> device,
    media::VideoCaptureDeviceFactory* factory)
    : device_(std::move(device)),
      device_started_(false),
      dxgi_device_manager_(factory ? factory->GetDxgiDeviceManager()
                                   : nullptr) {}

#else
DeviceMediaToMojoAdapter::DeviceMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureDevice> device)
    : device_(std::move(device)), device_started_(false) {}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

DeviceMediaToMojoAdapter::~DeviceMediaToMojoAdapter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_started_)
    device_->StopAndDeAllocate();
}

void DeviceMediaToMojoAdapter::Start(
    const media::VideoCaptureParams& requested_settings,
    mojo::PendingRemote<mojom::VideoFrameHandler>
        video_frame_handler_pending_remote) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "DeviceMediaToMojoAdapter::Start");
  StartInternal(std::move(requested_settings),
                std::move(video_frame_handler_pending_remote),
                /*frame_handler=*/nullptr, /*start_in_process=*/false,
                media::VideoEffectsContext({}));
}

void DeviceMediaToMojoAdapter::StartInProcess(
    const media::VideoCaptureParams& requested_settings,
    const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
    media::VideoEffectsContext context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "DeviceMediaToMojoAdapter::StartInProcess");

  StartInternal(std::move(requested_settings),
                /*handler_pending_remote=*/std::nullopt,
                std::move(frame_handler), /*start_in_process=*/true,
                std::move(context));
}

void DeviceMediaToMojoAdapter::StartInternal(
    const media::VideoCaptureParams& requested_settings,
    std::optional<mojo::PendingRemote<mojom::VideoFrameHandler>>
        handler_pending_remote,
    const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
    bool start_in_process,
    media::VideoEffectsContext context) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "DeviceMediaToMojoAdapter::StartInternal");
  std::unique_ptr<media::VideoFrameReceiverOnTaskRunner> media_receiver;
  base::WeakPtr<media::VideoFrameReceiver> video_frame_receiver;
  if (start_in_process) {
    DCHECK(frame_handler);
    media_receiver = std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
        frame_handler, base::SingleThreadTaskRunner::GetCurrentDefault());
    video_frame_receiver = frame_handler;
  } else {
    DCHECK(handler_pending_remote);
    mojo::Remote<mojom::VideoFrameHandler> handler_remote(
        std::move(*handler_pending_remote));
    handler_remote.set_disconnect_handler(base::BindOnce(
        &DeviceMediaToMojoAdapter::OnClientConnectionErrorOrClose,
        weak_factory_.GetWeakPtr()));

    receiver_ =
        std::make_unique<ReceiverMojoToMediaAdapter>(std::move(handler_remote));
    media_receiver = std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
        receiver_->GetWeakPtr(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    video_frame_receiver = receiver_->GetWeakPtr();
  }

  if (requested_settings.buffer_type !=
          media::VideoCaptureBufferType::kSharedMemory &&
      requested_settings.buffer_type !=
          media::VideoCaptureBufferType::kGpuMemoryBuffer) {
    // Buffer types other than shared memory are not supported.
    media_receiver->OnError(
        media::VideoCaptureError::
            kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType);
    return;
  }

  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool;
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
                 "CreateVideoCaptureBufferPoolImpl");
    // Create a dedicated buffer pool for the device usage session.
#if BUILDFLAG(IS_WIN)
    buffer_pool = base::MakeRefCounted<media::VideoCaptureBufferPoolImpl>(
        requested_settings.buffer_type, max_buffer_pool_buffer_count(),
        std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>(
            dxgi_device_manager_));
#else   // BUILDFLAG(IS_WIN)
    buffer_pool = base::MakeRefCounted<media::VideoCaptureBufferPoolImpl>(
        requested_settings.buffer_type, max_buffer_pool_buffer_count());
#endif  // !BUILDFLAG(IS_WIN)
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto device_client = std::make_unique<media::VideoCaptureDeviceClient>(
      std::move(media_receiver), buffer_pool,
      base::BindRepeating(
          &CreateGpuJpegDecoder, jpeg_decoder_task_runner_,
          jpeg_decoder_factory_callback_,
          base::BindPostTaskToCurrentDefault(base::BindRepeating(
              &media::VideoFrameReceiver::OnFrameReadyInBuffer,
              video_frame_receiver)),
          base::BindPostTaskToCurrentDefault(base::BindRepeating(
              &media::VideoFrameReceiver::OnLog, video_frame_receiver))));
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  auto device_client = std::make_unique<media::VideoCaptureDeviceClient>(
      std::move(media_receiver), buffer_pool, std::move(context));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  device_->AllocateAndStart(requested_settings, std::move(device_client));
  device_started_ = true;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  device_->GetPhotoState(base::BindOnce(&media::LogCaptureDeviceEffects));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
}

void DeviceMediaToMojoAdapter::StopInProcess() {
  DCHECK(thread_checker_.CalledOnValidThread());

  OnClientConnectionErrorOrClose();
}

void DeviceMediaToMojoAdapter::MaybeSuspend() {
  if (!device_started_)
    return;
  device_->MaybeSuspend();
}

void DeviceMediaToMojoAdapter::Resume() {
  if (!device_started_)
    return;
  device_->Resume();
}

void DeviceMediaToMojoAdapter::GetPhotoState(GetPhotoStateCallback callback) {
  media::VideoCaptureDevice::GetPhotoStateCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)), nullptr);
  device_->GetPhotoState(std::move(scoped_callback));
}

void DeviceMediaToMojoAdapter::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  media::mojom::ImageCapture::SetPhotoOptionsCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(std::move(callback)), false);
  device_->SetPhotoOptions(std::move(settings), std::move(scoped_callback));
}

void DeviceMediaToMojoAdapter::TakePhoto(TakePhotoCallback callback) {
  auto scoped_trace = ScopedCaptureTrace::CreateIfEnabled("TakePhoto");
  media::mojom::ImageCapture::TakePhotoCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindPostTaskToCurrentDefault(
              base::BindOnce(&TakePhotoCallbackTrampoline, std::move(callback),
                             std::move(scoped_trace))),
          nullptr);
  device_->TakePhoto(std::move(scoped_callback));
}

void DeviceMediaToMojoAdapter::ProcessFeedback(
    const media::VideoCaptureFeedback& feedback) {
  // Feedback ID may not propagated by mojo interface.
  device_->OnUtilizationReport(feedback);
}

void DeviceMediaToMojoAdapter::RequestRefreshFrame() {
  device_->RequestRefreshFrame();
}

void DeviceMediaToMojoAdapter::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("video_and_image_capture"),
               "DeviceMediaToMojoAdapter::Stop");
  if (!device_started_)
    return;
  device_started_ = false;
  weak_factory_.InvalidateWeakPtrs();
  device_->StopAndDeAllocate();
  if (receiver_) {
    // We need to post the deletion of receiver to the end of the message queue,
    // because |device_->StopAndDeAllocate()| may post messages (e.g.
    // OnBufferRetired()) to a WeakPtr to |receiver_| to this queue,
    // and we need those messages to be sent before we invalidate the WeakPtr.
    base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(receiver_));
  }
}

void DeviceMediaToMojoAdapter::OnClientConnectionErrorOrClose() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Stop();
}

// static
int DeviceMediaToMojoAdapter::max_buffer_pool_buffer_count() {
  return media::DeviceVideoCaptureMaxBufferPoolSize();
}

}  // namespace video_capture
