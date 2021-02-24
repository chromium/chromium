// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_SPEECH_RECOGNITION_MANAGER_H_
#define CONTENT_PUBLIC_TEST_FAKE_SPEECH_RECOGNITION_MANAGER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"

namespace content {

class SpeechRecognitionManagerDelegate;

// Fake SpeechRecognitionManager class that can be used for tests.
// By default the recognition manager will respond with "Pictures of the moon"
// as recognized result from speech. This result can be overridden by calling
// SetFakeResult().
class FakeSpeechRecognitionManager : public SpeechRecognitionManager,
                                     public SpeechRecognitionEventListener {
 public:
  FakeSpeechRecognitionManager();
  ~FakeSpeechRecognitionManager() override;

  const std::string& grammar() const {
    return grammar_;
  }

  bool did_cancel_all() {
    return did_cancel_all_;
  }

  void set_should_send_fake_response(bool send) {
    should_send_fake_response_ = send;
  }

  bool should_send_fake_response() {
    return should_send_fake_response_;
  }

  void WaitForRecognitionStarted();

  void SetFakeResult(const std::string& result);

  // SpeechRecognitionManager methods.
  int CreateSession(const SpeechRecognitionSessionConfig& config) override;
  void StartSession(int session_id) override;
  void AbortSession(int session_id) override;
  void StopAudioCaptureForSession(int session_id) override;
  void AbortAllSessionsForRenderFrame(int render_process_id,
                                      int render_frame_id) override;
  const SpeechRecognitionSessionConfig& GetSessionConfig(
      int session_id) override;
  SpeechRecognitionSessionContext GetSessionContext(int session_id) override;

  // SpeechRecognitionEventListener implementation.
  void OnRecognitionStart(int session_id) override {}
  void OnAudioStart(int session_id) override {}
  void OnEnvironmentEstimationComplete(int session_id) override {}
  void OnSoundStart(int session_id) override {}
  void OnSoundEnd(int session_id) override {}
  void OnAudioEnd(int session_id) override {}
  void OnRecognitionEnd(int session_id) override {}
  void OnRecognitionResults(
      int session_id,
      const std::vector<blink::mojom::SpeechRecognitionResultPtr>& result)
      override {}
  void OnRecognitionError(
      int session_id,
      const blink::mojom::SpeechRecognitionError& error) override {}
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override {}

  void SetDelegate(SpeechRecognitionManagerDelegate* delegate);

 private:
  void SetFakeRecognitionResult();

  int session_id_;
  SpeechRecognitionEventListener* listener_;
  SpeechRecognitionSessionConfig session_config_;
  SpeechRecognitionSessionContext session_ctx_;
  std::string fake_result_;
  std::string grammar_;
  bool did_cancel_all_;
  bool should_send_fake_response_;
  base::OnceClosure recognition_started_closure_;
  SpeechRecognitionManagerDelegate* delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(FakeSpeechRecognitionManager);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_SPEECH_RECOGNITION_MANAGER_H_
