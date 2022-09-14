// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_H_
#define MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_H_

#include "media/capture/video/video_capture_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// To ensure correct operation, this mock device holds on to the |client|
// that is passed to it in AllocateAndStart() and releases it on
// StopAndDeAllocate().
class MockDevice : public media::VideoCaptureDevice {
 public:
  MockDevice();
  ~MockDevice() override;

  void SendStubFrame(const media::VideoCaptureFormat& format,
                     int rotation,
                     int frame_feedback_id);
  void SendOnStarted();

  // media::VideoCaptureDevice implementation.
  MOCK_METHOD2(DoAllocateAndStart,
               void(const media::VideoCaptureParams& params,
                    std::unique_ptr<Client>* client));
  MOCK_METHOD0(RequestRefreshFrame, void());
  MOCK_METHOD0(DoStopAndDeAllocate, void());
  MOCK_METHOD1(DoGetPhotoState, void(GetPhotoStateCallback* callback));
  MOCK_METHOD2(DoSetPhotoOptions,
               void(media::mojom::PhotoSettingsPtr* settings,
                    SetPhotoOptionsCallback* callback));
  MOCK_METHOD1(DoTakePhoto, void(TakePhotoCallback* callback));
  MOCK_METHOD1(OnUtilizationReport, void(media::VideoCaptureFeedback));

  void AllocateAndStart(const media::VideoCaptureParams& params,
                        std::unique_ptr<Client> client) override;
  void StopAndDeAllocate() override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;

  std::unique_ptr<Client> TakeOutClient() { return std::move(client_); }

 private:
  std::unique_ptr<Client> client_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_MOCK_DEVICE_H_
