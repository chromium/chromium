// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TTS_ENVIRONMENT_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_TTS_ENVIRONMENT_ANDROID_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

class WebContents;

// Provides embedder specific information for text-to-speech.
class CONTENT_EXPORT TtsEnvironmentAndroid {
 public:
  virtual ~TtsEnvironmentAndroid() = default;

  // Returns whether utterances are allowed to speak from WebContents that are
  // hidden. Returning false prevents speaking utterance from hidden
  // WebContents, and also stops any playing utterance if the WebContents is
  // hidden.
  virtual bool CanSpeakUtterancesFromHiddenWebContents() = 0;

  // Returns true if speech is allowed at the current time. This is called
  // right before an utterance is about to be spoken.
  virtual bool CanSpeakNow() = 0;

  // Sets the callback that is notified when the value of CanSpeakNow() changes.
  virtual void SetCanSpeakNowChangedCallback(
      base::RepeatingClosure callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TTS_ENVIRONMENT_ANDROID_H_
