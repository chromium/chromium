// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_MAC_H_
#define CONTENT_BROWSER_SPEECH_TTS_MAC_H_

#include "base/functional/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/no_destructor.h"
#include "content/browser/speech/tts_platform_impl.h"

#import <Cocoa/Cocoa.h>

class TtsPlatformImplMac;

@interface ChromeTtsDelegate : NSObject <NSSpeechSynthesizerDelegate>

- (id)initWithPlatformImplMac:(TtsPlatformImplMac*)ttsImplMac;

@end

// Subclass of NSSpeechSynthesizer that takes an utterance
// string on initialization, retains it and only allows it
// to be spoken once.
//
// We construct a new NSSpeechSynthesizer for each utterance, for
// two reasons:
// 1. To associate delegate callbacks with a particular utterance,
//    without assuming anything undocumented about the protocol.
// 2. To work around https://openradar.appspot.com/13425549,
//    where Nuance voices don't retain the utterance string and
//    crash when trying to call willSpeakWord.
@interface SingleUseSpeechSynthesizer : NSSpeechSynthesizer

- (id)initWithUtterance:(NSString*)utterance;
- (bool)startSpeakingRetainedUtterance;
- (bool)startSpeakingString:(NSString*)utterance;

@end

class TtsPlatformImplMac : public content::TtsPlatformImpl {
 public:
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

  // Called by ChromeTtsDelegate when we get a callback from the
  // native speech engine.
  void OnSpeechEvent(NSSpeechSynthesizer* sender,
                     content::TtsEventType event_type,
                     int char_index,
                     int char_length,
                     const std::string& error_message);

  // Get the single instance of this class.
  CONTENT_EXPORT static TtsPlatformImplMac* GetInstance();

  CONTENT_EXPORT static std::vector<content::VoiceData>& VoicesRefForTesting();

 private:
  friend base::NoDestructor<TtsPlatformImplMac>;
  TtsPlatformImplMac();

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const content::VoiceData& voice,
                     const content::UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  base::scoped_nsobject<SingleUseSpeechSynthesizer> speech_synthesizer_;
  base::scoped_nsobject<ChromeTtsDelegate> delegate_;
  int utterance_id_ = -1;
  std::string utterance_;
  int last_char_index_ = 0;
  bool paused_ = false;
};

#endif  // CONTENT_BROWSER_SPEECH_TTS_MAC_H_
