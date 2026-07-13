// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_MOCK_CAPTURE_DEVICE_CONTROLLER_H_
#define CONTENT_BROWSER_WEBRTC_MOCK_CAPTURE_DEVICE_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/webrtc/mock_camera_device.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace content {

// Manages mock capture devices backed by the video-capture service's virtual
// device support.
//
// Sequence-affine; must be constructed, called, and destroyed on one
// caller-selected sequence. Owned MockCameraDevice instances live on that same
// sequence.
class MockCaptureDeviceController {
 public:
  using ConnectToVideoSourceProviderCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>)>;

  MockCaptureDeviceController(scoped_refptr<base::SequencedTaskRunner>
                                  video_capture_service_task_runner,
                              ConnectToVideoSourceProviderCallback
                                  connect_to_video_source_provider_callback);

  ~MockCaptureDeviceController();

  MockCaptureDeviceController(const MockCaptureDeviceController&) = delete;
  MockCaptureDeviceController& operator=(const MockCaptureDeviceController&) =
      delete;

  void AddMockCamera(MockCameraConfig config);
  void RemoveMockCamera(const std::string& device_id);
  void Reset();

 private:
  void EnsureConnectedToVideoSourceProvider();

  scoped_refptr<base::SequencedTaskRunner> video_capture_service_task_runner_;
  ConnectToVideoSourceProviderCallback
      connect_to_video_source_provider_callback_;

  mojo::Remote<video_capture::mojom::VideoSourceProvider> source_provider_;

  std::map<std::string, std::unique_ptr<MockCameraDevice>> cameras_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_MOCK_CAPTURE_DEVICE_CONTROLLER_H_
