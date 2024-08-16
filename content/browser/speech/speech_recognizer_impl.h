// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/speech/endpointer/endpointer.h"
#include "components/speech/speech_recognizer_fsm.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognizer.h"
#include "content/common/content_export.h"
#include "media/base/audio_capturer_source.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_audio_forwarder.mojom.h"
#include "media/mojo/mojom/speech_recognition_error.mojom.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {
class AudioBus;
class AudioSystem;
}  // namespace media

namespace content {

class SpeechRecognitionEventListener;
struct SpeechRecognitionAudioForwarderConfig;

// Handles speech recognition for a session (identified by |session_id|), taking
// care of audio capture, silence detection/endpointer and interaction with the
// SpeechRecognitionEngine.
class CONTENT_EXPORT SpeechRecognizerImpl
    : public SpeechRecognizer,
      public media::AudioCapturerSource::CaptureCallback,
      public SpeechRecognitionEngine::Delegate,
      public speech::SpeechRecognizerFsm,
      public media::mojom::SpeechRecognitionAudioForwarder {
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
                       std::unique_ptr<SpeechRecognitionEngine> engine,
                       std::optional<SpeechRecognitionAudioForwarderConfig>
                           audio_forwarder_config);

  SpeechRecognizerImpl(const SpeechRecognizerImpl&) = delete;
  SpeechRecognizerImpl& operator=(const SpeechRecognizerImpl&) = delete;

  // SpeechRecognizer methods.
  void StartRecognition(const std::string& device_id) override;
  void AbortRecognition() override;
  void StopAudioCapture() override;
  bool IsActive() const override;
  bool IsCapturingAudio() const override;

  const SpeechRecognitionEngine& recognition_engine() const;

 private:
  friend class SpeechRecognizerTest;

  ~SpeechRecognizerImpl() override;

  // Callback from AudioSystem.
  void OnAudioParametersReceived(
      const std::optional<media::AudioParameters>& params);

  // speech::SpeechRecognizerFsm implementation.
  // Process a new audio chunk in the audio pipeline (endpointer, vumeter, etc).
  void DispatchEvent(const FSMEventArgs& event_args) override;
  void ProcessAudioPipeline(const FSMEventArgs& event_args) override;
  FSMState PrepareRecognition(const FSMEventArgs&) override;
  FSMState StartRecording(const FSMEventArgs& event_args) override;
  FSMState StartRecognitionEngine(const FSMEventArgs& event_args) override;
  FSMState WaitEnvironmentEstimationCompletion(
      const FSMEventArgs& event_args) override;
  FSMState DetectUserSpeechOrTimeout(const FSMEventArgs& event_args) override;
  FSMState StopCaptureAndWaitForResult(const FSMEventArgs& event_args) override;
  FSMState ProcessIntermediateResult(const FSMEventArgs& event_args) override;
  FSMState ProcessFinalResult(const FSMEventArgs& event_args) override;
  FSMState AbortSilently(const FSMEventArgs& event_args) override;
  FSMState AbortWithError(const FSMEventArgs& event_args) override;
  FSMState Abort(const media::mojom::SpeechRecognitionError& error) override;
  FSMState DetectEndOfSpeech(const FSMEventArgs& event_args) override;
  FSMState DoNothing(const FSMEventArgs& event_args) const override;
  FSMState NotFeasible(const FSMEventArgs& event_args) override;

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
               const media::AudioGlitchInfo& glitch_info,
               double volume,
               bool key_pressed) final;
  void OnCaptureError(media::AudioCapturerSource::ErrorCode code,
                      const std::string& message) final;
  void OnCaptureMuted(bool is_muted) final {}

  // media::mojom::blink::SpeechRecognitionAudioForwarder methods.
  void AddAudioFromRenderer(media::mojom::AudioDataS16Ptr buffer) override;

  // SpeechRecognitionEngineDelegate methods.
  void OnSpeechRecognitionEngineResults(
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>& results)
      override;
  void OnSpeechRecognitionEngineEndOfUtterance() override;
  void OnSpeechRecognitionEngineError(
      const media::mojom::SpeechRecognitionError& error) override;

  media::AudioSystem* GetAudioSystem();
  void CreateAudioCapturerSource();
  media::AudioCapturerSource* GetAudioCapturerSource();

  // Substitute the real audio system and capturer source in browser tests.
  static media::AudioSystem* audio_system_for_tests_;
  static media::AudioCapturerSource* audio_capturer_source_for_tests_;

  raw_ptr<media::AudioSystem, DanglingUntriaged> audio_system_;
  std::unique_ptr<SpeechRecognitionEngine> recognition_engine_;
  int sample_rate_;
  speech::Endpointer endpointer_;
  scoped_refptr<media::AudioCapturerSource> audio_capturer_source_;
  int num_samples_recorded_;
  float audio_level_;
  bool provisional_results_;
  bool end_of_utterance_;
  std::string device_id_;
  media::AudioParameters audio_parameters_;
  bool use_audio_capturer_source_ = true;
  mojo::Receiver<media::mojom::SpeechRecognitionAudioForwarder>
      audio_forwarder_receiver_;
  media::AudioParameters device_params_;

  class OnDataConverter;

  // Converts data between native input format and a WebSpeech specific
  // output format.
  std::unique_ptr<SpeechRecognizerImpl::OnDataConverter> audio_converter_;

  base::WeakPtrFactory<SpeechRecognizerImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNIZER_IMPL_H_
