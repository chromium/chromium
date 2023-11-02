// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_PUSH_SUBSCRIPTION_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_PUSH_SUBSCRIPTION_H_

#include "media/capture/mojom/image_capture.mojom.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockPushSubcription
    : public video_capture::mojom::PushVideoStreamSubscription {
 public:
  MockPushSubcription();
  ~MockPushSubcription() override;
  void Suspend(SuspendCallback callback) override;
  void GetPhotoState(GetPhotoStateCallback callback) override;
  void SetPhotoOptions(media::mojom::PhotoSettingsPtr settings,
                       SetPhotoOptionsCallback callback) override;
  void TakePhoto(TakePhotoCallback callback) override;
  void Close(CloseCallback callback) override;

  MOCK_METHOD0(Activate, void());
  MOCK_METHOD1(DoSuspend, void(SuspendCallback& callback));
  MOCK_METHOD0(Resume, void());
  MOCK_METHOD1(DoGetPhotoState, void(GetPhotoStateCallback& callback));
  MOCK_METHOD2(DoSetPhotoOptions,
               void(media::mojom::PhotoSettingsPtr& settings,
                    SetPhotoOptionsCallback& callback));
  MOCK_METHOD1(DoTakePhoto, void(TakePhotoCallback& callback));
  MOCK_METHOD1(DoClose, void(CloseCallback& callback));
  MOCK_METHOD1(ProcessFeedback,
               void(const media::VideoCaptureFeedback& feedback));
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_PUSH_SUBSCRIPTION_H_
