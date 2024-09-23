// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_SHARED_MEMORY_VIRTUAL_DEVICE_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_SHARED_MEMORY_VIRTUAL_DEVICE_MOJO_ADAPTER_H_

#include "base/sequence_checker.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/device.h"
#include "services/video_capture/public/cpp/video_frame_access_handler.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"

namespace video_capture {

class SharedMemoryVirtualDeviceMojoAdapter
    : public mojom::SharedMemoryVirtualDevice,
      public Device {
 public:
  explicit SharedMemoryVirtualDeviceMojoAdapter(
      mojo::Remote<mojom::Producer> producer);

  SharedMemoryVirtualDeviceMojoAdapter(
      const SharedMemoryVirtualDeviceMojoAdapter&) = delete;
  SharedMemoryVirtualDeviceMojoAdapter& operator=(
      const SharedMemoryVirtualDeviceMojoAdapter&) = delete;

  ~SharedMemoryVirtualDeviceMojoAdapter() override;

  // mojom::SharedMemoryVirtualDevice implementation.
  void RequestFrameBuffer(const gfx::Size& dimension,
                          media::VideoPixelFormat pixel_format,
                          media::mojom::PlaneStridesPtr strides,
                          RequestFrameBufferCallback callback) override;
  void OnFrameReadyInBuffer(
      int32_t buffer_id,
      ::media::mojom::VideoFrameInfoPtr frame_info) override;

  // Device implementation.
  void Start(const media::VideoCaptureParams& requested_settings,
             mojo::PendingRemote<mojom::VideoFrameHandler> receiver) override;
  void StartInProcess(
      const media::VideoCaptureParams& requested_settings,
      const base::WeakPtr<media::VideoFrameReceiver>& frame_handler,
      media::VideoEffectsContext context) override;
  void MaybeSuspend() override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;
  void ProcessFeedback(const media::VideoCaptureFeedback& feedback) override;
  void RequestRefreshFrame() override;

  void Stop();

  // Returns the fixed maximum number of buffers passed to the constructor
  // of VideoCaptureBufferPoolImpl.
  static int max_buffer_pool_buffer_count();

 private:
  void OnReceiverConnectionErrorOrClose();

  mojo::Remote<mojom::VideoFrameHandler> video_frame_handler_;
  // Used when this device is started in process.
  base::WeakPtr<media::VideoFrameReceiver> video_frame_handler_in_process_;
  mojo::Remote<mojom::Producer> producer_;
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool_;
  std::vector<int> known_buffer_ids_;
  scoped_refptr<ScopedAccessPermissionMap> scoped_access_permission_map_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_SHARED_MEMORY_VIRTUAL_DEVICE_MOJO_ADAPTER_H_
