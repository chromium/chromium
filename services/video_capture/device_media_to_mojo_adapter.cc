// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_media_to_mojo_adapter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_switches.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/video_capture/receiver_mojo_to_media_adapter.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/scoped_video_capture_jpeg_decoder.h"
#include "media/capture/video/chromeos/video_capture_jpeg_decoder_impl.h"
#endif  // defined(OS_CHROMEOS)

namespace {

#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)

}  // anonymous namespace

namespace video_capture {

#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)

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
  auto buffer_tracker_factory =
      std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>();
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool(
      new media::VideoCaptureBufferPoolImpl(std::move(buffer_tracker_factory),
                                            requested_settings.buffer_type,
                                            max_buffer_pool_buffer_count()));
#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)

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
  // The maximum number of video frame buffers in-flight at any one time.
  // If all buffers are still in use by consumers when new frames are produced
  // those frames get dropped.
  static int kMaxBufferCount = 3;

#if defined(OS_CHROMEOS)
  // On Chrome OS with MIPI cameras running on HAL v3, there can be three
  // concurrent streams of camera pipeline depth ~6. We allow at most 24 buffers
  // here to take into account the delay caused by the consumer (e.g. display or
  // video encoder).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVideoCaptureUseGpuMemoryBuffer)) {
    kMaxBufferCount = 24;
  }
#endif

  return kMaxBufferCount;
}

}  // namespace video_capture
