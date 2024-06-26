// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "base/functional/bind.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"

namespace content {

// This is content_shell's delegate used by the speech recognition manager to
// check for permission to record audio. For content_shell, we always authorize
// speech recognition (see crbug.com/237119).
class ShellSpeechRecognitionManagerDelegate
    : public SpeechRecognitionManagerDelegate {
 public:
  ShellSpeechRecognitionManagerDelegate() {}

  ShellSpeechRecognitionManagerDelegate(
      const ShellSpeechRecognitionManagerDelegate&) = delete;
  ShellSpeechRecognitionManagerDelegate& operator=(
      const ShellSpeechRecognitionManagerDelegate&) = delete;

  ~ShellSpeechRecognitionManagerDelegate() override {}

  // SpeechRecognitionManagerDelegate methods.
  void CheckRecognitionIsAllowed(
      int session_id,
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback)
      override;
  SpeechRecognitionEventListener* GetEventListener() override;
#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
  // It is empty in this delegate.
  void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver)
      override;
#endif  //! BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
