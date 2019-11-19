// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_SHARED_MEMORY_VIRTUAL_DEVICE_MOJO_ADAPTER_H_
#define SERVICES_VIDEO_CAPTURE_SHARED_MEMORY_VIRTUAL_DEVICE_MOJO_ADAPTER_H_

#include "base/sequence_checker.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"

namespace video_capture {

class SharedMemoryVirtualDeviceMojoAdapter
    : public mojom::SharedMemoryVirtualDevice,
      public mojom::Device {
 public:
  SharedMemoryVirtualDeviceMojoAdapter(
      mojo::Remote<mojom::Producer> producer,
      bool send_buffer_handles_to_producer_as_raw_file_descriptors = false);
  ~SharedMemoryVirtualDeviceMojoAdapter() override;

  // mojom::SharedMemoryVirtualDevice implementation.
  void RequestFrameBuffer(const gfx::Size& dimension,
                          media::VideoPixelFormat pixel_format,
                          media::mojom::PlaneStridesPtr strides,
                          RequestFrameBufferCallback callback) override;
  void OnFrameReadyInBuffer(
      int32_t buffer_id,
      ::media::mojom::VideoFrameInfoPtr frame_info) override;

  // mojom::Device implementation.
  void Start(const media::VideoCaptureParams& requested_settings,
             mojo::PendingRemote<mojom::VideoFrameHandler> receiver) override;
  void MaybeSuspend() override;
  void Resume() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;

  void Stop();

  // Returns the fixed maximum number of buffers passed to the constructor
  // of VideoCaptureBufferPoolImpl.
  static int max_buffer_pool_buffer_count();

 private:
  void OnReceiverConnectionErrorOrClose();

  mojo::Remote<mojom::VideoFrameHandler> video_frame_handler_;
  mojo::Remote<mojom::Producer> producer_;
  const bool send_buffer_handles_to_producer_as_raw_file_descriptors_;
  scoped_refptr<media::VideoCaptureBufferPool> buffer_pool_;
  std::vector<int> known_buffer_ids_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SharedMemoryVirtualDeviceMojoAdapter);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_SHARED_MEMORY_VIRTUAL_DEVICE_MOJO_ADAPTER_H_
