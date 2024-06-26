// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "base/functional/callback_forward.h"

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
namespace media::mojom {
class SpeechRecognitionContext;
}
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)

namespace content {

class SpeechRecognitionEventListener;

// Allows embedders to display the current state of recognition, for getting the
// user's permission and for fetching optional request information.
class SpeechRecognitionManagerDelegate {
 public:
  virtual ~SpeechRecognitionManagerDelegate() = default;

  // Checks (asynchronously) if current setup allows speech recognition.
  // This is called on the IO thread.
  virtual void CheckRecognitionIsAllowed(
      int session_id,
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback) = 0;

  // Checks whether the delegate is interested (returning a non nullptr ptr) or
  // not (returning nullptr) in receiving a copy of all sessions events.
  // This is called on the IO thread.
  virtual SpeechRecognitionEventListener* GetEventListener() = 0;

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
  // This interfaces is used to bind the On-Device Speech Recognition Service on
  // the non-android platforms.
  // This is called on the IO thread.
  virtual void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext>
          receiver) = 0;
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
