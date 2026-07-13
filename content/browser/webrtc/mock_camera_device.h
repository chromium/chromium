// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_MOCK_CAMERA_DEVICE_H_
#define CONTENT_BROWSER_WEBRTC_MOCK_CAMERA_DEVICE_H_

#include <map>
#include <string>

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/video/video_capture_device_info.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace content {

struct MockCameraConfig {
  std::string device_id;
  std::string label;
  gfx::Size size = gfx::Size(640, 480);
  double frame_rate = 5.0;
};

// Represents one shared-memory-backed mock camera.
//
// Sequence-affine; must be constructed, called, and destroyed on the owning
// MockCaptureDeviceController's sequence.
class MockCameraDevice : public video_capture::mojom::Producer {
 public:
  explicit MockCameraDevice(MockCameraConfig config);
  ~MockCameraDevice() override;

  MockCameraDevice(const MockCameraDevice&) = delete;
  MockCameraDevice& operator=(const MockCameraDevice&) = delete;

  media::VideoCaptureDeviceInfo BuildDeviceInfo() const;

  mojo::PendingRemote<video_capture::mojom::Producer> BindProducerRemote();

  mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
  BindDeviceReceiver();

  void StartProducingFrames();

 private:
  // video_capture::mojom::Producer:
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle,
                   OnNewBufferCallback callback) override;
  void OnBufferRetired(int32_t buffer_id) override;

  void PushFrame();
  void OnFrameBufferReady(int32_t buffer_id);
  void ScheduleNextFrame();

  MockCameraConfig config_;

  mojo::Receiver<video_capture::mojom::Producer> producer_receiver_{this};
  mojo::Remote<video_capture::mojom::SharedMemoryVirtualDevice> device_;

  std::map<int32_t, base::WritableSharedMemoryMapping> buffers_;

  base::TimeTicks start_time_;
  base::TimeTicks last_frame_request_time_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MockCameraDevice> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_MOCK_CAMERA_DEVICE_H_
