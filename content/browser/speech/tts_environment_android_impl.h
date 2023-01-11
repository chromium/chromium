// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_ENVIRONMENT_ANDROID_IMPL_H_
#define CONTENT_BROWSER_SPEECH_TTS_ENVIRONMENT_ANDROID_IMPL_H_

#include <memory>

#include "base/android/application_status_listener.h"
#include "base/functional/callback.h"
#include "content/public/browser/tts_environment_android.h"

namespace content {

// Default implementation of TtsEnvironment that is used if the embedder
// doesn't supply one. This uses ApplicationStatus to stop speech when the
// application no longer has visible activities and does not allow speech
// if there are no visible activities.
class TtsEnvironmentAndroidImpl : public TtsEnvironmentAndroid {
 public:
  TtsEnvironmentAndroidImpl();
  TtsEnvironmentAndroidImpl(const TtsEnvironmentAndroidImpl&) = delete;
  TtsEnvironmentAndroidImpl& operator=(const TtsEnvironmentAndroidImpl&) =
      delete;
  ~TtsEnvironmentAndroidImpl() override;

  // TtsEnvironment:
  bool CanSpeakUtterancesFromHiddenWebContents() override;
  bool CanSpeakNow() override;
  void SetCanSpeakNowChangedCallback(base::RepeatingClosure callback) override;

 private:
  void OnApplicationStateChanged(base::android::ApplicationState state);

  base::RepeatingClosure can_speak_now_changed_callback_;
  std::unique_ptr<base::android::ApplicationStatusListener>
      application_status_listener_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_ENVIRONMENT_ANDROID_IMPL_H_
