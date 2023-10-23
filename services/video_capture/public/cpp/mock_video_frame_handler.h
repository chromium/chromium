// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_FRAME_HANDLER_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_FRAME_HANDLER_H_

#include <vector>

#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/mojo/mojom/media_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockVideoFrameHandler : public mojom::VideoFrameHandler {
 public:
  explicit MockVideoFrameHandler(
      mojo::PendingReceiver<mojom::VideoFrameHandler> handler);
  ~MockVideoFrameHandler() override;

  void HoldAccessPermissions();
  void ReleaseAccessedFrames();

  // Use forwarding method to work around gmock not supporting move-only types.
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameAccessHandlerReady(
      mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
          frame_access_handler_proxy) override;
  void OnFrameReadyInBuffer(mojom::ReadyFrameInBufferPtr buffer) override;
  void OnBufferRetired(int32_t buffer_id) override;

  MOCK_METHOD0(OnCaptureConfigurationChanged, void());
  MOCK_METHOD2(DoOnNewBuffer,
               void(int32_t, media::mojom::VideoBufferHandlePtr*));
  MOCK_METHOD3(DoOnFrameReadyInBuffer,
               void(int32_t buffer_id,
                    int32_t frame_feedback_id,
                    media::mojom::VideoFrameInfoPtr*));
  MOCK_METHOD1(DoOnBufferRetired, void(int32_t));
  MOCK_METHOD1(OnError, void(media::VideoCaptureError));
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason));
  MOCK_METHOD1(OnNewSubCaptureTargetVersion, void(uint32_t));
  MOCK_METHOD0(OnFrameWithEmptyRegionCapture, void());
  MOCK_METHOD1(OnLog, void(const std::string&));
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStartedUsingGpuDecode, void());
  MOCK_METHOD0(OnStopped, void());

 private:
  const mojo::Receiver<mojom::VideoFrameHandler> video_frame_handler_;
  std::vector<int32_t> known_buffer_ids_;
  bool should_store_access_permissions_;
  mojo::Remote<video_capture::mojom::VideoFrameAccessHandler>
      frame_access_handler_;
  std::vector<int32_t> accessed_frame_ids_;
};

class FakeVideoFrameAccessHandler : public mojom::VideoFrameAccessHandler {
 public:
  FakeVideoFrameAccessHandler();
  explicit FakeVideoFrameAccessHandler(
      base::RepeatingCallback<void(int32_t)> callback);
  ~FakeVideoFrameAccessHandler() override;

  const std::vector<int32_t>& released_buffer_ids() const;
  void ClearReleasedBufferIds();

  // mojom::VideoFrameAccessHandler implementation.
  void OnFinishedConsumingBuffer(int32_t buffer_id) override;

 private:
  std::vector<int32_t> released_buffer_ids_;
  base::RepeatingCallback<void(int32_t)> callback_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_FRAME_HANDLER_H_
