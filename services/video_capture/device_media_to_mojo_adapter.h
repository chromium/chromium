// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_MEDIA_TO_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_MEDIA_TO_MOJO_ADAPTER_H_

#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/capture/video/video_capture_device.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_capture/device.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/capture/video/chromeos/video_capture_device_factory_chromeos.h"
#include "media/capture/video/chromeos/video_capture_jpeg_decoder.h"
#elif BUILDFLAG(IS_WIN)
#include "media/capture/video/win/video_capture_device_factory_win.h"
#endif

namespace video_capture {

class ReceiverMojoToMediaAdapter;

// Implementation of mojom::Device backed by a given instance of
// media::VideoCaptureDevice.
class DeviceMediaToMojoAdapter : public Device {
 public:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DeviceMediaToMojoAdapter(
      std::unique_ptr<media::VideoCaptureDevice> device,
      media::MojoMjpegDecodeAcceleratorFactoryCB jpeg_decoder_factory_callback,
      scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner);
#elif BUILDFLAG(IS_WIN)
  DeviceMediaToMojoAdapter(std::unique_ptr<media::VideoCaptureDevice> device,
                           media::VideoCaptureDeviceFactory* factory);
#else
  DeviceMediaToMojoAdapter(
      std::unique_ptr<media::VideoCaptureDevice> device);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  ~DeviceMediaToMojoAdapter() override;

  // Device implementation.
  void Start(const media::VideoCaptureParams& requested_settings,
             mojo::PendingRemote<mojom::VideoFrameHandler>
                 handler_pending_remote) override;
  void StartInProcess(
      const media::VideoCaptureParams& requested_settings,
      const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
      media::VideoEffectsContext context) override;
  void StopInProcess() override;
  void MaybeSuspend() override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback) override;
  void RequestRefreshFrame() override;

  void Stop();
  void OnClientConnectionErrorOrClose();

  // Returns the fixed maximum number of buffers passed to the constructor
  // of VideoCaptureBufferPoolImpl.
  static int max_buffer_pool_buffer_count();

 private:
  void StartInternal(
      const media::VideoCaptureParams& requested_settings,
      std::optional<mojo::PendingRemote<mojom::VideoFrameHandler>>
          handler_pending_remote,
      const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
      bool start_in_process,
      media::VideoEffectsContext context);

  const std::unique_ptr<media::VideoCaptureDevice> device_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const media::MojoMjpegDecodeAcceleratorFactoryCB
      jpeg_decoder_factory_callback_;
  scoped_refptr<base::SequencedTaskRunner> jpeg_decoder_task_runner_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ReceiverMojoToMediaAdapter> receiver_;
  bool device_started_ = false;
#if BUILDFLAG(IS_WIN)
  scoped_refptr<media::DXGIDeviceManager> dxgi_device_manager_;
#endif
  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<DeviceMediaToMojoAdapter> weak_factory_{this};
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_MEDIA_TO_MOJO_ADAPTER_H_
