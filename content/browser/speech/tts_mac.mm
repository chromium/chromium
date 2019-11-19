// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "content/browser/speech/tts_platform_impl.h"
#include "content/public/browser/tts_controller.h"

#import <Cocoa/Cocoa.h>

class TtsPlatformImplMac;

@interface ChromeTtsDelegate : NSObject <NSSpeechSynthesizerDelegate> {
 @private
  TtsPlatformImplMac* ttsImplMac_;  // weak.
}

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
// 2. To work around http://openradar.appspot.com/radar?id=2854403,
//    where Nuance voices don't retain the utterance string and
//    crash when trying to call willSpeakWord.
@interface SingleUseSpeechSynthesizer : NSSpeechSynthesizer {
 @private
  base::scoped_nsobject<NSString> utterance_;
  bool didSpeak_;
}

- (id)initWithUtterance:(NSString*)utterance;
- (bool)startSpeakingRetainedUtterance;
- (bool)startSpeakingString:(NSString*)utterance;

@end

class TtsPlatformImplMac : public content::TtsPlatformImpl {
 public:
  bool PlatformImplAvailable() override { return true; }

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
  static TtsPlatformImplMac* GetInstance();

 private:
  TtsPlatformImplMac();
  ~TtsPlatformImplMac() override;

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const content::VoiceData& voice,
                     const content::UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  base::scoped_nsobject<SingleUseSpeechSynthesizer> speech_synthesizer_;
  base::scoped_nsobject<ChromeTtsDelegate> delegate_;
  int utterance_id_;
  std::string utterance_;
  int last_char_index_;
  bool paused_;

  friend struct base::DefaultSingletonTraits<TtsPlatformImplMac>;

  base::WeakPtrFactory<TtsPlatformImplMac> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TtsPlatformImplMac);
};

// static
content::TtsPlatformImpl* content::TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplMac::GetInstance();
}

void TtsPlatformImplMac::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  // Parse SSML and process speech.
  content::TtsController::GetInstance()->StripSSML(
      utterance, base::BindOnce(&TtsPlatformImplMac::ProcessSpeech,
                                weak_factory_.GetWeakPtr(), utterance_id, lang,
                                voice, params, std::move(on_speak_finished)));
}

void TtsPlatformImplMac::ProcessSpeech(
    int utterance_id,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished,
    const std::string& parsed_utterance) {
  utterance_ = parsed_utterance;
  paused_ = false;

  NSString* utterance_nsstring =
      [NSString stringWithUTF8String:utterance_.c_str()];
  if (!utterance_nsstring) {
    std::move(on_speak_finished).Run(false);
    return;
  }

  // Deliberately construct a new speech synthesizer every time Speak is
  // called, otherwise there's no way to know whether calls to the delegate
  // apply to the current utterance or a previous utterance. In
  // experimentation, the overhead of constructing and destructing a
  // NSSpeechSynthesizer is minimal.
  speech_synthesizer_.reset([[SingleUseSpeechSynthesizer alloc]
      initWithUtterance:utterance_nsstring]);
  [speech_synthesizer_ setDelegate:delegate_];

  if (!voice.native_voice_identifier.empty()) {
    NSString* native_voice_identifier =
        [NSString stringWithUTF8String:voice.native_voice_identifier.c_str()];
    [speech_synthesizer_ setVoice:native_voice_identifier];
  }

  utterance_id_ = utterance_id;

  // TODO: support languages other than the default: crbug.com/88059

  if (params.rate >= 0.0) {
    // The TTS api defines rate via words per minute. Let 200 be the default.
    [speech_synthesizer_ setObject:[NSNumber numberWithInt:params.rate * 200]
                       forProperty:NSSpeechRateProperty
                             error:nil];
  }

  if (params.pitch >= 0.0) {
    // The input is a float from 0.0 to 2.0, with 1.0 being the default.
    // Get the default pitch for this voice and modulate it by 50% - 150%.
    NSError* errorCode;
    NSNumber* defaultPitchObj =
        [speech_synthesizer_ objectForProperty:NSSpeechPitchBaseProperty
                                         error:&errorCode];
    int defaultPitch = defaultPitchObj ? [defaultPitchObj intValue] : 48;
    int newPitch = static_cast<int>(defaultPitch * (0.5 * params.pitch + 0.5));
    [speech_synthesizer_ setObject:[NSNumber numberWithInt:newPitch]
                       forProperty:NSSpeechPitchBaseProperty
                             error:nil];
  }

  if (params.volume >= 0.0) {
    [speech_synthesizer_ setObject:[NSNumber numberWithFloat:params.volume]
                       forProperty:NSSpeechVolumeProperty
                             error:nil];
  }

  bool success = [speech_synthesizer_ startSpeakingRetainedUtterance];
  if (success) {
    content::TtsController* controller = content::TtsController::GetInstance();
    controller->OnTtsEvent(utterance_id_, content::TTS_EVENT_START, 0, -1, "");
  }
  std::move(on_speak_finished).Run(success);
}

bool TtsPlatformImplMac::StopSpeaking() {
  if (speech_synthesizer_.get()) {
    [speech_synthesizer_ stopSpeaking];
    speech_synthesizer_.reset(nil);
  }
  paused_ = false;
  return true;
}

void TtsPlatformImplMac::Pause() {
  if (speech_synthesizer_.get() && utterance_id_ && !paused_) {
    [speech_synthesizer_ pauseSpeakingAtBoundary:NSSpeechImmediateBoundary];
    paused_ = true;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_PAUSE, last_char_index_, -1, "");
  }
}

void TtsPlatformImplMac::Resume() {
  if (speech_synthesizer_.get() && utterance_id_ && paused_) {
    [speech_synthesizer_ continueSpeaking];
    paused_ = false;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_RESUME, last_char_index_, -1, "");
  }
}

bool TtsPlatformImplMac::IsSpeaking() {
  if (speech_synthesizer_)
    return [speech_synthesizer_ isSpeaking];
  return false;
}

void TtsPlatformImplMac::GetVoices(std::vector<content::VoiceData>* outVoices) {
  NSArray* voices = [NSSpeechSynthesizer availableVoices];

  // Create a new temporary array of the available voices with
  // the default voice first.
  NSMutableArray* orderedVoices =
      [NSMutableArray arrayWithCapacity:[voices count]];
  NSString* defaultVoice = [NSSpeechSynthesizer defaultVoice];
  if (defaultVoice) {
    [orderedVoices addObject:defaultVoice];
  }
  for (NSString* voiceIdentifier in voices) {
    if (![voiceIdentifier isEqualToString:defaultVoice])
      [orderedVoices addObject:voiceIdentifier];
  }

  for (NSString* voiceIdentifier in orderedVoices) {
    outVoices->push_back(content::VoiceData());
    content::VoiceData& data = outVoices->back();

    NSDictionary* attributes =
        [NSSpeechSynthesizer attributesForVoice:voiceIdentifier];
    NSString* name = [attributes objectForKey:NSVoiceName];
    NSString* localeIdentifier =
        [attributes objectForKey:NSVoiceLocaleIdentifier];

    data.native = true;
    data.native_voice_identifier = base::SysNSStringToUTF8(voiceIdentifier);
    data.name = base::SysNSStringToUTF8(name);

    NSDictionary* localeComponents =
        [NSLocale componentsFromLocaleIdentifier:localeIdentifier];
    NSString* language = [localeComponents objectForKey:NSLocaleLanguageCode];
    NSString* country = [localeComponents objectForKey:NSLocaleCountryCode];
    if (language && country) {
      data.lang = base::SysNSStringToUTF8(
          [NSString stringWithFormat:@"%@-%@", language, country]);
    } else {
      data.lang = base::SysNSStringToUTF8(language);
    }
    data.events.insert(content::TTS_EVENT_START);
    data.events.insert(content::TTS_EVENT_END);
    data.events.insert(content::TTS_EVENT_WORD);
    data.events.insert(content::TTS_EVENT_ERROR);
    data.events.insert(content::TTS_EVENT_CANCELLED);
    data.events.insert(content::TTS_EVENT_INTERRUPTED);
    data.events.insert(content::TTS_EVENT_PAUSE);
    data.events.insert(content::TTS_EVENT_RESUME);
  }
}

void TtsPlatformImplMac::OnSpeechEvent(NSSpeechSynthesizer* sender,
                                       content::TtsEventType event_type,
                                       int char_index,
                                       int char_length,
                                       const std::string& error_message) {
  // Don't send events from an utterance that's already completed.
  // This depends on the fact that we construct a new NSSpeechSynthesizer
  // each time we call Speak.
  if (sender != speech_synthesizer_.get())
    return;

  if (event_type == content::TTS_EVENT_END)
    char_index = utterance_.size();

  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, event_type, char_index, char_length, error_message);
  last_char_index_ = char_index;
}

TtsPlatformImplMac::TtsPlatformImplMac() : weak_factory_(this) {
  utterance_id_ = -1;
  paused_ = false;

  delegate_.reset([[ChromeTtsDelegate alloc] initWithPlatformImplMac:this]);
}

TtsPlatformImplMac::~TtsPlatformImplMac() {}

// static
TtsPlatformImplMac* TtsPlatformImplMac::GetInstance() {
  return base::Singleton<TtsPlatformImplMac>::get();
}

@implementation ChromeTtsDelegate

- (id)initWithPlatformImplMac:(TtsPlatformImplMac*)ttsImplMac {
  if ((self = [super init])) {
    ttsImplMac_ = ttsImplMac;
  }
  return self;
}

- (void)speechSynthesizer:(NSSpeechSynthesizer*)sender
        didFinishSpeaking:(BOOL)finished_speaking {
  ttsImplMac_->OnSpeechEvent(sender, content::TTS_EVENT_END, 0, -1, "");
}

- (void)speechSynthesizer:(NSSpeechSynthesizer*)sender
            willSpeakWord:(NSRange)word_range
                 ofString:(NSString*)string {
  // Ignore bogus word_range. The Mac speech synthesizer is a bit
  // buggy and occasionally returns a number way out of range.
  if (word_range.location > [string length])
    return;

  ttsImplMac_->OnSpeechEvent(sender, content::TTS_EVENT_WORD,
                             word_range.location, word_range.length, "");
}

- (void)speechSynthesizer:(NSSpeechSynthesizer*)sender
    didEncounterErrorAtIndex:(NSUInteger)character_index
                    ofString:(NSString*)string
                     message:(NSString*)message {
  // Ignore bogus character_index. The Mac speech synthesizer is a bit
  // buggy and occasionally returns a number way out of range.
  if (character_index > [string length])
    return;

  std::string message_utf8 = base::SysNSStringToUTF8(message);
  ttsImplMac_->OnSpeechEvent(sender, content::TTS_EVENT_ERROR, character_index,
                             -1, message_utf8);
}

@end

@implementation SingleUseSpeechSynthesizer

- (id)initWithUtterance:(NSString*)utterance {
  self = [super init];
  if (self) {
    utterance_.reset([utterance retain]);
    didSpeak_ = false;
  }
  return self;
}

- (bool)startSpeakingRetainedUtterance {
  CHECK(!didSpeak_);
  CHECK(utterance_);
  didSpeak_ = true;
  return [super startSpeakingString:utterance_];
}

- (bool)startSpeakingString:(NSString*)utterance {
  CHECK(false);
  return false;
}

@end
