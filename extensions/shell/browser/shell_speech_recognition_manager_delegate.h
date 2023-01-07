// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager_delegate.h"

namespace extensions {
namespace speech {

class ShellSpeechRecognitionManagerDelegate
    : public content::SpeechRecognitionManagerDelegate,
      public content::SpeechRecognitionEventListener {
 public:
  ShellSpeechRecognitionManagerDelegate();

  ShellSpeechRecognitionManagerDelegate(
      const ShellSpeechRecognitionManagerDelegate&) = delete;
  ShellSpeechRecognitionManagerDelegate& operator=(
      const ShellSpeechRecognitionManagerDelegate&) = delete;

  ~ShellSpeechRecognitionManagerDelegate() override;

 private:
  // SpeechRecognitionEventListener methods.
  void OnRecognitionStart(int session_id) override;
  void OnAudioStart(int session_id) override;
  void OnEnvironmentEstimationComplete(int session_id) override;
  void OnSoundStart(int session_id) override;
  void OnSoundEnd(int session_id) override;
  void OnAudioEnd(int session_id) override;
  void OnRecognitionEnd(int session_id) override;
  void OnRecognitionResults(
      int session_id,
      const std::vector<blink::mojom::SpeechRecognitionResultPtr>& result)
      override;
  void OnRecognitionError(
      int session_id,
      const blink::mojom::SpeechRecognitionError& error) override;
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override;

  // SpeechRecognitionManagerDelegate methods.
  void CheckRecognitionIsAllowed(
      int session_id,
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback)
      override;
  content::SpeechRecognitionEventListener* GetEventListener() override;
  bool FilterProfanities(int render_process_id) override;

  static void CheckRenderFrameType(
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback,
      int render_process_id,
      int render_frame_id);
};

}  // namespace speech
}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
