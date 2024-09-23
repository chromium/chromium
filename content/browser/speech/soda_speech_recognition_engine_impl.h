// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SODA_SPEECH_RECOGNITION_ENGINE_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SODA_SPEECH_RECOGNITION_ENGINE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/common/content_export.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class SpeechRecognitionManagerDelegate;

// This is the on-device implementation for `SpeechRecognitionEngine`.
//
// This class establishes a connection to the on-device speech recognition
// service using the content::SpeechRecognitionManagerDelegate. It will bind to
// the speech::CrosSpeechRecognitionService in ChromeOS-Ash. On LaCrOS, it will
// forward to Ash. On other platforms, it will bind to the
// speech::ChromeSpeechRecognitionService if the on-device speech recognition
// service is available. This class will be in the speech recognition available
// state when successfully bound.

class CONTENT_EXPORT SodaSpeechRecognitionEngineImpl
    : public SpeechRecognitionEngine,
      public media::mojom::SpeechRecognitionRecognizerClient {
 public:
  using SendAudioToSpeechRecognitionServiceCallback =
      base::RepeatingCallback<void(media::mojom::AudioDataS16Ptr audio_data)>;

  explicit SodaSpeechRecognitionEngineImpl(
      const SpeechRecognitionSessionConfig& config);
  ~SodaSpeechRecognitionEngineImpl() override;
  SodaSpeechRecognitionEngineImpl(const SodaSpeechRecognitionEngineImpl&) =
      delete;
  SodaSpeechRecognitionEngineImpl& operator=(
      const SodaSpeechRecognitionEngineImpl&) = delete;

  // Sets the delegate for tests.
  static void SetSpeechRecognitionManagerDelegateForTesting(
      SpeechRecognitionManagerDelegate*);

  bool Initialize();
  void SetOnReadyCallback(base::OnceCallback<void()> callback);

  // content::SodaSpeechRecognitionEngineImpl:
  void StartRecognition() override;
  void EndRecognition() override;
  void TakeAudioChunk(const AudioChunk& data) override;
  void AudioChunksEnded() override;
  int GetDesiredAudioChunkDurationMs() const override;

  // media::mojom::SpeechRecognitionRecognizerClient:
  void OnSpeechRecognitionRecognitionEvent(
      const media::SpeechRecognitionResult& result,
      OnSpeechRecognitionRecognitionEventCallback reply) override;
  void OnSpeechRecognitionError() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;
  void OnSpeechRecognitionStopped() override;

 private:
  // Callback executed when the recognizer is bound. Sets the flag indicating
  // whether the speech recognition service supports multichannel audio.
  void OnRecognizerBound(bool is_multichannel_supported);

  // Called when the speech recognition context or the speech recognition
  // recognizer is disconnected. Sends an error message to the UI and halts
  // future transcriptions.
  void OnRecognizerDisconnected();

  void SendAudioToSpeechRecognitionService(
      media::mojom::AudioDataS16Ptr audio_data);

  void MarkDone();

  void Abort(media::mojom::SpeechRecognitionErrorCode error);

  media::mojom::AudioDataS16Ptr ConvertToAudioDataS16(const AudioChunk& data);

  base::OnceCallback<void()> on_ready_callback_;

  // Sends audio to the speech recognition thread on the main thread.
  SendAudioToSpeechRecognitionServiceCallback send_audio_callback_;

  base::RepeatingCallback<void()> mark_done_callback_;

  mojo::Remote<media::mojom::SpeechRecognitionContext>
      speech_recognition_context_;
  mojo::Remote<media::mojom::SpeechRecognitionRecognizer>
      speech_recognition_recognizer_;
  mojo::Receiver<media::mojom::SpeechRecognitionRecognizerClient>
      speech_recognition_recognizer_client_{this};

  SpeechRecognitionSessionConfig config_;

  SEQUENCE_CHECKER(main_sequence_checker_);

  // A flag indicating the recognition state.
  bool is_start_recognition_ = false;

  base::WeakPtrFactory<SodaSpeechRecognitionEngineImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SODA_SPEECH_RECOGNITION_ENGINE_IMPL_H_
