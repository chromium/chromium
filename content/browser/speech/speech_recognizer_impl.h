// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "content/browser/speech/endpointer/endpointer.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognizer.h"
#include "media/base/audio_capturer_source.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_error.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

namespace media {
class AudioBus;
class AudioSystem;
}  // namespace media

namespace content {

class SpeechRecognitionEventListener;

// Handles speech recognition for a session (identified by |session_id|), taking
// care of audio capture, silence detection/endpointer and interaction with the
// SpeechRecognitionEngine.
class CONTENT_EXPORT SpeechRecognizerImpl
    : public SpeechRecognizer,
      public media::AudioCapturerSource::CaptureCallback,
      public SpeechRecognitionEngine::Delegate {
 public:
  static constexpr int kAudioSampleRate = 16000;
  static constexpr media::ChannelLayout kChannelLayout =
      media::CHANNEL_LAYOUT_MONO;
  static constexpr int kNumBitsPerAudioSample = 16;
  static constexpr int kNoSpeechTimeoutMs = 8000;
  static constexpr int kEndpointerEstimationTimeMs = 300;

  static void SetAudioEnvironmentForTesting(
      media::AudioSystem* audio_system,
      media::AudioCapturerSource* capturer_source);

  SpeechRecognizerImpl(SpeechRecognitionEventListener* listener,
                       media::AudioSystem* audio_system,
                       int session_id,
                       bool continuous,
                       bool provisional_results,
                       SpeechRecognitionEngine* engine);

  // SpeechRecognizer methods.
  void StartRecognition(const std::string& device_id) override;
  void AbortRecognition() override;
  void StopAudioCapture() override;
  bool IsActive() const override;
  bool IsCapturingAudio() const override;

  const SpeechRecognitionEngine& recognition_engine() const;

 private:
  friend class SpeechRecognizerTest;

  enum FSMState {
    STATE_IDLE = 0,
    STATE_PREPARING,
    STATE_STARTING,
    STATE_ESTIMATING_ENVIRONMENT,
    STATE_WAITING_FOR_SPEECH,
    STATE_RECOGNIZING,
    STATE_WAITING_FINAL_RESULT,
    STATE_ENDED,
    STATE_MAX_VALUE = STATE_ENDED
  };

  enum FSMEvent {
    EVENT_ABORT = 0,
    EVENT_PREPARE,
    EVENT_START,
    EVENT_STOP_CAPTURE,
    EVENT_AUDIO_DATA,
    EVENT_ENGINE_RESULT,
    EVENT_ENGINE_ERROR,
    EVENT_AUDIO_ERROR,
    EVENT_MAX_VALUE = EVENT_AUDIO_ERROR
  };

  struct FSMEventArgs {
    explicit FSMEventArgs(FSMEvent event_value);
    FSMEventArgs(const FSMEventArgs& other);
    ~FSMEventArgs();

    FSMEvent event;
    scoped_refptr<AudioChunk> audio_data;
    std::vector<blink::mojom::SpeechRecognitionResultPtr> engine_results;
    blink::mojom::SpeechRecognitionError engine_error;
  };

  ~SpeechRecognizerImpl() override;

  // Entry point for pushing any new external event into the recognizer FSM.
  void DispatchEvent(const FSMEventArgs& event_args);

  // Defines the behavior of the recognizer FSM, selecting the appropriate
  // transition according to the current state and event.
  FSMState ExecuteTransitionAndGetNextState(const FSMEventArgs& args);

  // Process a new audio chunk in the audio pipeline (endpointer, vumeter, etc).
  void ProcessAudioPipeline(const AudioChunk& raw_audio);

  // Callback from AudioSystem.
  void OnDeviceInfo(const base::Optional<media::AudioParameters>& params);

  // The methods below handle transitions of the recognizer FSM.
  FSMState PrepareRecognition(const FSMEventArgs&);
  FSMState StartRecording(const FSMEventArgs& event_args);
  FSMState StartRecognitionEngine(const FSMEventArgs& event_args);
  FSMState WaitEnvironmentEstimationCompletion(const FSMEventArgs& event_args);
  FSMState DetectUserSpeechOrTimeout(const FSMEventArgs& event_args);
  FSMState StopCaptureAndWaitForResult(const FSMEventArgs& event_args);
  FSMState ProcessIntermediateResult(const FSMEventArgs& event_args);
  FSMState ProcessFinalResult(const FSMEventArgs& event_args);
  FSMState AbortSilently(const FSMEventArgs& event_args);
  FSMState AbortWithError(const FSMEventArgs& event_args);
  FSMState Abort(const blink::mojom::SpeechRecognitionError& error);
  FSMState DetectEndOfSpeech(const FSMEventArgs& event_args);
  FSMState DoNothing(const FSMEventArgs& event_args) const;
  FSMState NotFeasible(const FSMEventArgs& event_args);

  // Returns the time span of captured audio samples since the start of capture.
  int GetElapsedTimeMs() const;

  // Calculates the input volume to be displayed in the UI, triggering the
  // OnAudioLevelsChange event accordingly.
  void UpdateSignalAndNoiseLevels(const float& rms, bool clip_detected);

  void CloseAudioCapturerSource();

  // media::AudioCapturerSource::CaptureCallback methods.
  void OnCaptureStarted() final {}
  void Capture(const media::AudioBus* audio_bus,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) final;
  void OnCaptureError(const std::string& message) final;
  void OnCaptureMuted(bool is_muted) final {}

  // SpeechRecognitionEngineDelegate methods.
  void OnSpeechRecognitionEngineResults(
      const std::vector<blink::mojom::SpeechRecognitionResultPtr>& results)
      override;
  void OnSpeechRecognitionEngineEndOfUtterance() override;
  void OnSpeechRecognitionEngineError(
      const blink::mojom::SpeechRecognitionError& error) override;

  media::AudioSystem* GetAudioSystem();
  void CreateAudioCapturerSource();
  media::AudioCapturerSource* GetAudioCapturerSource();

  // Substitute the real audio system and capturer source in browser tests.
  static media::AudioSystem* audio_system_for_tests_;
  static media::AudioCapturerSource* audio_capturer_source_for_tests_;

  media::AudioSystem* audio_system_;
  std::unique_ptr<SpeechRecognitionEngine> recognition_engine_;
  Endpointer endpointer_;
  scoped_refptr<media::AudioCapturerSource> audio_capturer_source_;
  int num_samples_recorded_;
  float audio_level_;
  bool is_dispatching_event_;
  bool provisional_results_;
  bool end_of_utterance_;
  FSMState state_;
  std::string device_id_;
  media::AudioParameters device_params_;

  class OnDataConverter;

  // Converts data between native input format and a WebSpeech specific
  // output format.
  std::unique_ptr<SpeechRecognizerImpl::OnDataConverter> audio_converter_;

  base::WeakPtrFactory<SpeechRecognizerImpl> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SpeechRecognizerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
