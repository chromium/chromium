// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import "content/browser/speech/tts_mac.h"

#include <string>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/tts_controller.h"

namespace {

std::vector<content::VoiceData>& VoicesRef() {
  static base::NoDestructor<std::vector<content::VoiceData>> voices([]() {
    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationWillBecomeActiveNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification* notification) {
                  // The user might have switched to Settings or some other app
                  // to change voices or locale settings. Avoid a stale cache by
                  // forcing a rebuild of the voices vector after the app
                  // becomes active.
                  VoicesRef().clear();
                }];
    return std::vector<content::VoiceData>();
  }());

  return *voices;
}

std::vector<content::VoiceData>& Voices() {
  std::vector<content::VoiceData>& voices = VoicesRef();

  if (!voices.empty())
    return voices;

  base::scoped_nsobject<NSMutableArray> voiceIdentifiers(
      [NSSpeechSynthesizer.availableVoices mutableCopy]);

  NSString* defaultVoice = NSSpeechSynthesizer.defaultVoice;
  if (defaultVoice) {
    [voiceIdentifiers removeObject:defaultVoice];
    [voiceIdentifiers insertObject:defaultVoice atIndex:0];
  }

  voices.reserve([voiceIdentifiers count]);

  for (NSString* voiceIdentifier in voiceIdentifiers.get()) {
    voices.push_back(content::VoiceData());
    content::VoiceData& data = voices.back();

    NSDictionary* attributes =
        [NSSpeechSynthesizer attributesForVoice:voiceIdentifier];
    NSString* name = attributes[NSVoiceName];
    NSString* localeIdentifier = attributes[NSVoiceLocaleIdentifier];

    data.native = true;
    data.native_voice_identifier = base::SysNSStringToUTF8(voiceIdentifier);
    data.name = base::SysNSStringToUTF8(name);

    NSDictionary* localeComponents =
        [NSLocale componentsFromLocaleIdentifier:localeIdentifier];
    NSString* language = localeComponents[NSLocaleLanguageCode];
    NSString* country = localeComponents[NSLocaleCountryCode];
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

  return voices;
}

}  // namespace

// static
content::TtsPlatformImpl* content::TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplMac::GetInstance();
}

TtsPlatformImplMac::~TtsPlatformImplMac() = default;

bool TtsPlatformImplMac::PlatformImplSupported() {
  return true;
}

bool TtsPlatformImplMac::PlatformImplInitialized() {
  return true;
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
                                base::Unretained(this), utterance_id, lang,
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
  *outVoices = Voices();
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

TtsPlatformImplMac::TtsPlatformImplMac() {
  delegate_.reset([[ChromeTtsDelegate alloc] initWithPlatformImplMac:this]);
}

// static
TtsPlatformImplMac* TtsPlatformImplMac::GetInstance() {
  static base::NoDestructor<TtsPlatformImplMac> tts_platform;
  return tts_platform.get();
}

// static
std::vector<content::VoiceData>& TtsPlatformImplMac::VoicesRefForTesting() {
  return VoicesRef();
}

@implementation ChromeTtsDelegate {
 @private
  raw_ptr<TtsPlatformImplMac> _ttsImplMac;  // weak.
}

- (id)initWithPlatformImplMac:(TtsPlatformImplMac*)ttsImplMac {
  if ((self = [super init])) {
    _ttsImplMac = ttsImplMac;
  }
  return self;
}

- (void)speechSynthesizer:(NSSpeechSynthesizer*)sender
        didFinishSpeaking:(BOOL)finished_speaking {
  _ttsImplMac->OnSpeechEvent(sender, content::TTS_EVENT_END, 0, -1, "");
}

- (void)speechSynthesizer:(NSSpeechSynthesizer*)sender
            willSpeakWord:(NSRange)word_range
                 ofString:(NSString*)string {
  // Ignore bogus word_range. The Mac speech synthesizer is a bit
  // buggy and occasionally returns a number way out of range.
  if (word_range.location > [string length])
    return;

  _ttsImplMac->OnSpeechEvent(sender, content::TTS_EVENT_WORD,
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
  _ttsImplMac->OnSpeechEvent(sender, content::TTS_EVENT_ERROR, character_index,
                             -1, message_utf8);
}

@end

@implementation SingleUseSpeechSynthesizer {
 @private
  base::scoped_nsobject<NSString> _utterance;
  bool _didSpeak;
}

- (id)initWithUtterance:(NSString*)utterance {
  self = [super init];
  if (self) {
    _utterance.reset([utterance retain]);
    _didSpeak = false;
  }
  return self;
}

- (bool)startSpeakingRetainedUtterance {
  CHECK(!_didSpeak);
  CHECK(_utterance);
  _didSpeak = true;
  return [super startSpeakingString:_utterance];
}

- (bool)startSpeakingString:(NSString*)utterance {
  CHECK(false);
  return false;
}

@end
