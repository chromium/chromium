// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/cardboard/cardboard_sdk_impl.h"

#include "base/android/jni_android.h"
#include "base/functional/callback_helpers.h"
#include "device/vr/android/xr_activity_state_handler.h"
#include "third_party/cardboard/src/sdk/include/cardboard.h"

using base::android::AttachCurrentThread;

namespace device {

CardboardSdkImpl::CardboardSdkImpl() = default;
CardboardSdkImpl::~CardboardSdkImpl() = default;

void CardboardSdkImpl::Initialize(jobject context) {
  if (initialized_) {
    return;
  }

  Cardboard_initializeAndroid(base::android::GetVM(), context);
  initialized_ = true;
}

void CardboardSdkImpl::ScanQrCodeAndSaveDeviceParams() {
  CHECK(initialized_);

  // Launches a new QR code scanner activity in order to scan a QR code with
  // the parameters of a new Cardboard viewer. Whether the QR code with
  // Cardboard viewer paramerter is scanned or the scanning is skipped, the
  // scanner activity is finished.
  CardboardQrCode_scanQrCodeAndSaveDeviceParams();
}

void CardboardSdkImpl::ScanQrCodeAndSaveDeviceParams(
    std::unique_ptr<XrActivityStateHandler> activity_state_handler,
    base::OnceClosure on_params_saved) {
  CHECK(initialized_);

  // Given that we will launch the QR code activity, we need to set the activity
  // on resume callback so as to ensure the app flows as expected.
  activity_state_handler_ = std::move(activity_state_handler);
  on_params_saved_ = std::move(on_params_saved);
  activity_state_handler_->SetResumedHandler(base::BindRepeating(
      &CardboardSdkImpl::OnActivityResumed, weak_ptr_factory_.GetWeakPtr()));

  // Launches a new QR code scanner activity in order to scan a QR code with
  // the parameters of a new Cardboard viewer. Whether the QR code with
  // Cardboard viewer paramerter is scanned or the scanning is skipped, the
  // scanner activity is finished.
  CardboardQrCode_scanQrCodeAndSaveDeviceParams();
}

void CardboardSdkImpl::OnActivityResumed() {
  // Currently we only care about being called back once, so reset the activity
  // handler here and then continue with our queued work.
  activity_state_handler_.reset();
  if (on_params_saved_) {
    std::move(on_params_saved_).Run();
  }
}

}  // namespace device
