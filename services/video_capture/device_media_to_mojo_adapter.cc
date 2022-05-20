// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_media_to_mojo_adapter.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_pool_util.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/video_capture/public/cpp/receiver_mojo_to_media_adapter.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/scoped_video_capture_jpeg_decoder.h"
#include "media/capture/video/chromeos/video_capture_jpeg_decoder_impl.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

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

}  // anonymous namespace

namespace video_capture {

#if BUILDFLAG(IS_CHROMEOS_ASH)
DeviceMediaToMojoAdapter::DeviceMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureDevice> device,
    media::MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_callback,
    scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner)
    : device_(std::move(device)),
      jpeg_decoder_factory_callback_(std::move(jpeg_decoder_factory_callback)),
      jpeg_decoder_task_runner_(std::move(jpeg_decoder_task_runner)),
      device_started_(false) {}
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
  mojo::Remote<mojom::VideoFrameHandler> handler_remote(
      std::move(video_frame_handler_pending_remote));
  handler_remote.set_disconnect_handler(
      base::BindOnce(&DeviceMediaToMojoAdapter::OnClientConnectionErrorOrClose,
                     weak_factory_.GetWeakPtr()));

  receiver_ =
      std::make_unique<ReceiverMojoToMediaAdapter>(std::move(handler_remote));
  auto media_receiver = std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
      receiver_->GetWeakPtr(), base::ThreadTaskRunnerHandle::Get());

  if (requested_settings.buffer_type !=
          media::VideoCaptureBufferType::kSharedMemory &&
      requested_settings.buffer_type !=
          media::VideoCaptureBufferType::kGpuMemoryBuffer &&
      requested_settings.buffer_type !=
          media::VideoCaptureBufferType::kSharedMemoryViaRawFileDescriptor) {
    // Buffer types other than shared memory are not supported.
    media_receiver->OnError(
        media::VideoCaptureError::
            kDeviceMediaToMojoAdapterEncounteredUnsupportedBufferType);
    return;
  }

  // Create a dedicated buffer pool for the device usage session.
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool(
      new media::VideoCaptureBufferPoolImpl(requested_settings.buffer_type,
                                            max_buffer_pool_buffer_count()));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto device_client = std::make_unique<media::VideoCaptureDeviceClient>(
      requested_settings.buffer_type, std::move(media_receiver), buffer_pool,
      base::BindRepeating(
          &CreateGpuJpegDecoder, jpeg_decoder_task_runner_,
          jpeg_decoder_factory_callback_,
          media::BindToCurrentLoop(base::BindRepeating(
              &media::VideoFrameReceiver::OnFrameReadyInBuffer,
              receiver_->GetWeakPtr())),
          media::BindToCurrentLoop(base::BindRepeating(
              &media::VideoFrameReceiver::OnLog, receiver_->GetWeakPtr()))));
#else
  auto device_client = std::make_unique<media::VideoCaptureDeviceClient>(
      requested_settings.buffer_type, std::move(media_receiver), buffer_pool);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  device_->AllocateAndStart(requested_settings, std::move(device_client));
  device_started_ = true;
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
          media::BindToCurrentLoop(std::move(callback)), nullptr);
  device_->GetPhotoState(std::move(scoped_callback));
}

void DeviceMediaToMojoAdapter::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  media::mojom::ImageCapture::SetOptionsCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          media::BindToCurrentLoop(std::move(callback)), false);
  device_->SetPhotoOptions(std::move(settings), std::move(scoped_callback));
}

void DeviceMediaToMojoAdapter::TakePhoto(TakePhotoCallback callback) {
  media::mojom::ImageCapture::TakePhotoCallback scoped_callback =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          media::BindToCurrentLoop(std::move(callback)), nullptr);
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
  if (!device_started_)
    return;
  device_started_ = false;
  weak_factory_.InvalidateWeakPtrs();
  device_->StopAndDeAllocate();
  // We need to post the deletion of receiver to the end of the message queue,
  // because |device_->StopAndDeAllocate()| may post messages (e.g.
  // OnBufferRetired()) to a WeakPtr to |receiver_| to this queue,
  // and we need those messages to be sent before we invalidate the WeakPtr.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                  std::move(receiver_));
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
