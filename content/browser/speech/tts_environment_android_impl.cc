// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_environment_android_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"

using base::android::ApplicationStatusListener;

namespace content {

TtsEnvironmentAndroidImpl::TtsEnvironmentAndroidImpl() = default;

TtsEnvironmentAndroidImpl::~TtsEnvironmentAndroidImpl() = default;

bool TtsEnvironmentAndroidImpl::CanSpeakUtterancesFromHiddenWebContents() {
  return true;
}

bool TtsEnvironmentAndroidImpl::CanSpeakNow() {
  return ApplicationStatusListener::HasVisibleActivities();
}

void TtsEnvironmentAndroidImpl::SetCanSpeakNowChangedCallback(
    base::RepeatingClosure callback) {
  if (!application_status_listener_) {
    application_status_listener_ =
        ApplicationStatusListener::New(base::BindRepeating(
            &TtsEnvironmentAndroidImpl::OnApplicationStateChanged,
            base::Unretained(this)));
  }
  can_speak_now_changed_callback_ = std::move(callback);
}

void TtsEnvironmentAndroidImpl::OnApplicationStateChanged(
    base::android::ApplicationState state) {
  if (can_speak_now_changed_callback_)
    can_speak_now_changed_callback_.Run();
}

}  // namespace content
