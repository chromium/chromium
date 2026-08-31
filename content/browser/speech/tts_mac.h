// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_MAC_H_
#define CONTENT_BROWSER_SPEECH_TTS_MAC_H_

#import <AVFAudio/AVFAudio.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/speech/tts_platform_impl.h"

class TtsPlatformImplMac;

class CONTENT_EXPORT TtsPlatformImplMacBackgroundWorker {
 public:
  TtsPlatformImplMacBackgroundWorker() = default;
  TtsPlatformImplMacBackgroundWorker(
      const TtsPlatformImplMacBackgroundWorker&) = delete;
  TtsPlatformImplMacBackgroundWorker& operator=(
      const TtsPlatformImplMacBackgroundWorker&) = delete;
  ~TtsPlatformImplMacBackgroundWorker() = default;

  // The installed voices as content::VoiceData, with the system default voice
  // first, plus that voice's identifier (empty if there is none).
  struct Voices {
    Voices();
    Voices(Voices&&);
    Voices& operator=(Voices&&);
    ~Voices();

    std::vector<content::VoiceData> voices;
    std::string default_voice_identifier;
  };

  AVSpeechSynthesisVoice* GetSystemDefaultVoice();
  std::string GetSystemDefaultVoiceIdentifier();

  // Enumerates the installed voices. AVSpeechSynthesisVoice.speechVoices can
  // take hundreds of milliseconds the first time it is called in a process, so
  // this must not run on the UI thread.
  Voices LoadVoices();
};

@interface ChromeTtsDelegate : NSObject <AVSpeechSynthesizerDelegate>

- (instancetype)initWithPlatformImplMac:(TtsPlatformImplMac*)ttsImplMac;

@end

class TtsPlatformImplMac : public content::TtsPlatformImpl {
 public:
  static constexpr int kInvalidUtteranceId = -1;

  ~TtsPlatformImplMac() override;

  TtsPlatformImplMac(const TtsPlatformImplMac&) = delete;
  TtsPlatformImplMac& operator=(const TtsPlatformImplMac&) = delete;

  bool PlatformImplSupported() override;
  bool PlatformImplInitialized() override;

  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const content::VoiceData& voice,
             const content::UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override;

  bool StopSpeaking() override;

  void Pause() override;

  void Resume() override;

  bool IsSpeaking() override;

  void GetVoices(std::vector<content::VoiceData>* out_voices) override;

  void RefreshVoices() override;

  // Called by ChromeTtsDelegate when we get a callback from the
  // native speech engine.
  void OnSpeechEvent(int utterance_id,
                     content::TtsEventType event_type,
                     int char_index,
                     int char_length,
                     const std::string& error_message);

  // Get the single instance of this class.
  CONTENT_EXPORT static TtsPlatformImplMac* GetInstance();

  CONTENT_EXPORT static base::SequenceBound<TtsPlatformImplMacBackgroundWorker>&
  GetBackgroundWorker();

 private:
  friend base::NoDestructor<TtsPlatformImplMac>;
  TtsPlatformImplMac();

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const content::VoiceData& voice,
                     const content::UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  // Starts (re)loading the voice list on the background worker. Requests made
  // while a load is in flight are coalesced into one more load after it.
  void LoadVoices();
  void OnVoicesLoaded(TtsPlatformImplMacBackgroundWorker::Voices voices);

  // Checks whether the system default voice changed (the user may have picked
  // another one in System Settings while away) and reloads the list if so.
  void UpdateSystemDefaultVoice();
  void OnGotDefaultVoiceIdentifier(std::string default_voice_identifier);
  void OnApplicationWillBecomeActive();

  SEQUENCE_CHECKER(sequence_checker_);

  AVSpeechSynthesizer* __strong speech_synthesizer_;
  ChromeTtsDelegate* __strong delegate_;
  id __strong application_active_observer_ = nil;
  int utterance_id_ = kInvalidUtteranceId;
  std::string utterance_;
  int last_char_index_ = 0;
  bool paused_ = false;

  // Empty until the first LoadVoices() completes; PlatformImplInitialized()
  // is false until then, so TtsController queues utterances and reports no
  // platform voices in the meantime.
  std::vector<content::VoiceData> voices_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::string default_voice_identifier_ GUARDED_BY_CONTEXT(sequence_checker_);
  bool voices_loaded_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_loading_voices_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool needs_reload_voices_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool is_updating_default_voice_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  bool received_voices_request_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
};

#endif  // CONTENT_BROWSER_SPEECH_TTS_MAC_H_
