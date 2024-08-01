// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_SPEECH_RECOGNITION_MANAGER_H_
#define CONTENT_PUBLIC_TEST_FAKE_SPEECH_RECOGNITION_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/speech_recognition_audio_forwarder_config.h"
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

  FakeSpeechRecognitionManager(const FakeSpeechRecognitionManager&) = delete;
  FakeSpeechRecognitionManager& operator=(const FakeSpeechRecognitionManager&) =
      delete;

  ~FakeSpeechRecognitionManager() override;

  const std::string& grammar() const {
    return grammar_;
  }

  bool did_cancel_all() const { return did_cancel_all_; }

  void set_should_send_fake_response(bool send) {
    should_send_fake_response_ = send;
  }

  bool should_send_fake_response() const { return should_send_fake_response_; }

  bool is_recognizing() const { return is_recognizing_; }

  void WaitForRecognitionStarted();
  void WaitForRecognitionEnded();

  // |end_recognition| means recognition will be ended and session state cleared
  // after sending the fake response.
  void SendFakeResponse(bool end_recognition,
                        base::OnceClosure on_fake_response_sent);

  void SetFakeResult(const std::string& result, bool is_final);

  void SendFakeError(base::OnceClosure on_fake_error_sent);

  // SpeechRecognitionManager methods.
  int CreateSession(const SpeechRecognitionSessionConfig& config) override;
  int CreateSession(
      const SpeechRecognitionSessionConfig& config,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
          session_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          client_remote,
      std::optional<SpeechRecognitionAudioForwarderConfig>
          audio_forwarder_config) override;
  void StartSession(int session_id) override;
  void AbortSession(int session_id) override;
  void StopAudioCaptureForSession(int session_id) override;
  void AbortAllSessionsForRenderFrame(int render_process_id,
                                      int render_frame_id) override;
  const SpeechRecognitionSessionConfig& GetSessionConfig(
      int session_id) override;
  SpeechRecognitionSessionContext GetSessionContext(int session_id) override;
  bool UseOnDeviceSpeechRecognition(
      const SpeechRecognitionSessionConfig& config) override;

  // SpeechRecognitionEventListener implementation.
  void OnRecognitionStart(int session_id) override {}
  void OnAudioStart(int session_id) override {}
  void OnSoundStart(int session_id) override {}
  void OnSoundEnd(int session_id) override {}
  void OnAudioEnd(int session_id) override {}
  void OnRecognitionEnd(int session_id) override {}
  void OnRecognitionResults(
      int session_id,
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& result)
      override {}
  void OnRecognitionError(
      int session_id,
      const media::mojom::SpeechRecognitionError& error) override {}
  void OnAudioLevelsChange(int session_id,
                           float volume,
                           float noise_volume) override {}

  void SetDelegate(SpeechRecognitionManagerDelegate* delegate);

 private:
  void OnRecognitionStarted();
  void OnRecognitionEnded();
  void OnFakeResponseSent();
  void SetFakeRecognitionResult(bool end_recognition);
  void SendFakeSpeechRecognitionError();
  void OnFakeErrorSent();

  int session_id_;
  raw_ptr<SpeechRecognitionEventListener, AcrossTasksDanglingUntriaged>
      listener_;
  SpeechRecognitionSessionConfig session_config_;
  SpeechRecognitionSessionContext session_ctx_;
  std::string fake_result_;
  bool is_final_ = true;
  std::string grammar_;
  bool did_cancel_all_ = false;
  bool should_send_fake_response_ = true;
  bool has_sent_result_ = false;
  bool is_recognizing_ = false;
  base::OnceClosure recognition_started_closure_;
  base::OnceClosure recognition_ended_closure_;
  base::OnceClosure on_fake_response_sent_closure_;
  base::OnceClosure on_fake_error_sent_closure_;
  raw_ptr<SpeechRecognitionManagerDelegate> delegate_ = nullptr;  // Not owned.
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_SPEECH_RECOGNITION_MANAGER_H_
