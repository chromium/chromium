// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBRTC_MOCK_CAPTURE_DEVICE_CONTROLLER_H_
#define CONTENT_BROWSER_WEBRTC_MOCK_CAPTURE_DEVICE_CONTROLLER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/webrtc/mock_camera_device.h"
#include "content/common/content_export.h"
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
class CONTENT_EXPORT MockCaptureDeviceController {
 public:
  using ConnectToVideoSourceProviderCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>)>;
  enum class AddMockCameraResult {
    kSuccess,
    kAlreadyExists,
    kInvalidDeviceInfo,
    kHardwareDeviceIdCollision,
    kNotSupported,
    kAborted,
  };
  using AddMockCameraCallback = base::OnceCallback<void(AddMockCameraResult)>;

  MockCaptureDeviceController(scoped_refptr<base::SequencedTaskRunner>
                                  video_capture_service_task_runner,
                              ConnectToVideoSourceProviderCallback
                                  connect_to_video_source_provider_callback);

  ~MockCaptureDeviceController();

  MockCaptureDeviceController(const MockCaptureDeviceController&) = delete;
  MockCaptureDeviceController& operator=(const MockCaptureDeviceController&) =
      delete;

  // Adds a camera if this controller does not already manage its device ID.
  // An ID that is registered or being registered is rejected without changing
  // the existing camera or registration attempt.
  // If supplied, |callback| is posted to this sequence. It receives kSuccess
  // after service registration, kAlreadyExists for a duplicate ID, or the
  // registration failure reason. Disconnection and cancellation report
  // kAborted. It does not wait for renderer device updates or capture.
  void AddMockCamera(MockCameraConfig config,
                     AddMockCameraCallback callback = {});
  void RemoveMockCamera(const std::string& device_id);
  void Reset();

 private:
  struct CameraEntry {
    std::unique_ptr<MockCameraDevice> camera;
    uint64_t registration_id = 0;
    AddMockCameraCallback callback;
  };

  void EnsureConnectedToVideoSourceProvider();
  void OnCameraRegistered(
      const std::string& device_id,
      uint64_t registration_id,
      video_capture::mojom::VideoSourceProvider::AddVirtualDeviceResult result);
  void OnVideoSourceProviderDisconnected();

  scoped_refptr<base::SequencedTaskRunner> video_capture_service_task_runner_;
  ConnectToVideoSourceProviderCallback
      connect_to_video_source_provider_callback_;

  mojo::Remote<video_capture::mojom::VideoSourceProvider> source_provider_;

  std::map<std::string, CameraEntry> cameras_;
  uint64_t next_registration_id_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<MockCaptureDeviceController> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBRTC_MOCK_CAPTURE_DEVICE_CONTROLLER_H_
