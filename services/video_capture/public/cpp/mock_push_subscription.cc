// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_push_subscription.h"

namespace video_capture {

MockPushSubcription::MockPushSubcription() = default;

MockPushSubcription::~MockPushSubcription() = default;

void MockPushSubcription::Suspend(SuspendCallback callback) {
  DoSuspend(callback);
}

void MockPushSubcription::GetPhotoState(GetPhotoStateCallback callback) {
  DoGetPhotoState(callback);
}

void MockPushSubcription::SetPhotoOptions(
    media::mojom::PhotoSettingsPtr settings,
    SetPhotoOptionsCallback callback) {
  DoSetPhotoOptions(settings, callback);
}

void MockPushSubcription::TakePhoto(TakePhotoCallback callback) {
  DoTakePhoto(callback);
}

void MockPushSubcription::Close(CloseCallback callback) {
  DoClose(callback);
}

}  // namespace video_capture
