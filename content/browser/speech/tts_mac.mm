// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/speech/tts_mac.h"

#import <AVFAudio/AVFAudio.h>
#import <AppKit/AppKit.h>
#include <objc/runtime.h>

#include <algorithm>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/tts_controller.h"

namespace {

constexpr int kNoLength = -1;
constexpr char kNoError[] = "";

std::vector<content::VoiceData>& VoicesRef() {
  static base::NoDestructor<std::vector<content::VoiceData>> voices([]() {
    [NSNotificationCenter.defaultCenter
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

AVSpeechSynthesisVoice* GetSystemDefaultVoice() {
  // This should be
  //
  //   [AVSpeechSynthesisVoice voiceWithLanguage:nil]
  //
  // but that has a bug (https://crbug.com/1484940#c9, FB13197951). In short,
  // while passing nil to -[AVSpeechSynthesisVoice voiceWithLanguage:] does
  // indeed return "the default voice for the system’s language and region",
  // that's not necessarily the voice that the user selected in System Settings
  // > Accessibility > Spoken Content, and that user voice selection is the only
  // one that matters. There does not appear to be an AVSpeechSynthesis API that
  // returns that user choice, so use the deprecated NSSpeechSynthesizer API,
  // which behaves correctly.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  NSString* default_voice_identifier = NSSpeechSynthesizer.defaultVoice;
#pragma clang diagnostic pop
  return [AVSpeechSynthesisVoice voiceWithIdentifier:default_voice_identifier];
}

std::vector<content::VoiceData>& Voices() {
  std::vector<content::VoiceData>& voices = VoicesRef();
  if (!voices.empty()) {
    return voices;
  }

  NSMutableArray* av_speech_voices =
      [[AVSpeechSynthesisVoice.speechVoices sortedArrayUsingDescriptors:@[
        [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES]
      ]] mutableCopy];
  AVSpeechSynthesisVoice* default_voice = GetSystemDefaultVoice();
  if (default_voice) {
    [av_speech_voices removeObject:default_voice];
    [av_speech_voices insertObject:default_voice atIndex:0];
  }

  // For the case of multiple voices with the same name but of a different
  // language, the old API (NSSpeechSynthesizer) would append locale information
  // to the names, while this current API does not. Because returning a bunch of
  // voices with the same name isn't helpful, count how often each name is used,
  // so that later on, locale information can be appended if necessary for
  // disambiguation.
  NSMutableDictionary<NSString*, NSNumber*>* name_counts =
      [NSMutableDictionary dictionary];
  for (AVSpeechSynthesisVoice* av_speech_voice in av_speech_voices) {
    NSString* voice_name = av_speech_voice.name;
    if (!voice_name) {
      // AVSpeechSynthesisVoice.name is not a nullable property, but there are
      // crashes (https://crbug.com/1459235) where -setObject:forKeyedSubscript:
      // is being passed a nil key, and the only place that happens in this
      // function is below.
      continue;
    }
    if (NSNumber* count = name_counts[voice_name]) {
      name_counts[voice_name] = @(count.intValue + 1);
    } else {
      name_counts[voice_name] = @1;
    }
  }

  voices.reserve(av_speech_voices.count);
  for (AVSpeechSynthesisVoice* av_speech_voice in av_speech_voices) {
    NSString* voice_name = av_speech_voice.name;
    if (!voice_name) {
      // AVSpeechSynthesisVoice.name is not a nullable property, but there are
      // crashes (https://crbug.com/1459235) where it seems like it's returning
      // nil. Without a name, a voice is useless, so skip it.
      continue;
    }

    voices.emplace_back();
    content::VoiceData& data = voices.back();

    if (name_counts[voice_name].intValue > 1) {
      // The language property on a voice is a BCP 47 code (i.e. "en-US") while
      // an NSLocale locale identifier isn't (i.e. "en_US"). However, using the
      // BCP 47 code as if it were a locale identifier works just fine (tested
      // back to 10.15).
      NSString* localized_language = [NSLocale.autoupdatingCurrentLocale
          localizedStringForLocaleIdentifier:av_speech_voice.language];
      voice_name = [NSString
          stringWithFormat:@"%@ (%@)", voice_name, localized_language];
    }

    data.native = true;
    data.native_voice_identifier =
        base::SysNSStringToUTF8(av_speech_voice.identifier);
    data.name = base::SysNSStringToUTF8(voice_name);
    data.lang = base::SysNSStringToUTF8(av_speech_voice.language);

    data.events.insert(content::TTS_EVENT_START);
    data.events.insert(content::TTS_EVENT_END);
    data.events.insert(content::TTS_EVENT_WORD);
    data.events.insert(content::TTS_EVENT_PAUSE);
    data.events.insert(content::TTS_EVENT_RESUME);
  }

  return voices;
}

AVSpeechUtterance* MakeUtterance(int utterance_id,
                                 const std::string& utterance_string) {
  AVSpeechUtterance* utterance = [AVSpeechUtterance
      speechUtteranceWithString:base::SysUTF8ToNSString(utterance_string)];
  objc_setAssociatedObject(utterance, @selector(identifier), @(utterance_id),
                           OBJC_ASSOCIATION_RETAIN);
  return utterance;
}

int GetUtteranceId(AVSpeechUtterance* utterance) {
  NSNumber* identifier = base::apple::ObjCCast<NSNumber>(
      objc_getAssociatedObject(utterance, @selector(identifier)));
  if (identifier) {
    return identifier.intValue;
  }
  return TtsPlatformImplMac::kInvalidUtteranceId;
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
  // Parse SSML and process speech. TODO(crbug.com/40273591):
  // AVSpeechUtterance has an initializer -initWithSSMLRepresentation:. Should
  // that be used instead?
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
  utterance_id_ = utterance_id;

  AVSpeechUtterance* speech_utterance =
      MakeUtterance(utterance_id, parsed_utterance);
  if (!speech_utterance) {
    std::move(on_speak_finished).Run(false);
    return;
  }

  speech_utterance.voice = [AVSpeechSynthesisVoice
      voiceWithIdentifier:base::SysUTF8ToNSString(
                              voice.native_voice_identifier)];

  if (params.rate >= 0.0) {
    // The two relevant APIs have different ranges:
    // - Web Speech API is [.1, 10] with default 1
    // - AVSpeechSynthesizer is [0, 1] with default .5
    //
    // Speeds in the Web Speech API other than 1 (the default rate) are meant to
    // be multiples of the default speaking rate.
    //
    // The mapping of AVSpeechSynthesizer speeds was done experimentally, using
    // the fourth paragraph of _A Tale of Two Cities_. With the "Samantha"
    // voice, AVSpeechUtteranceDefaultSpeechRate takes about 80s to read the
    // paragraph, while AVSpeechUtteranceMaximumSpeechRate takes about 20s.
    // Therefore, map
    //
    // 1 → AVSpeechUtteranceDefaultSpeechRate
    // 4 → AVSpeechUtteranceMaximumSpeechRate
    //
    // and cap anything higher.
    //
    // References:
    //
    // https://developer.mozilla.org/en-US/docs/Web/API/SpeechSynthesisUtterance/rate
    // https://github.com/WebKit/WebKit/blob/main/Source/WebCore/platform/cocoa/PlatformSpeechSynthesizerCocoa.mm
    //  ^ This is the WebKit implementation. It appears to have a bug in
    //    scaling, where a Web Speech API rate of 2 is scaled to
    //    AVSpeechUtteranceMaximumSpeechRate and the value passed to the
    //    AVSpeechSynthesizer goes up from there. A bug was filed about this:
    //    https://bugs.webkit.org/show_bug.cgi?id=258587

    float rate = params.rate;
    if (rate < 1) {
      // If a slower than normal rate is requested, scale the default speech
      // rate down proportionally.
      rate *= AVSpeechUtteranceDefaultSpeechRate;
    } else {
      // Scale the AVSpeech rate headroom proportionally to match the excess
      // above 1 in the Speech API, capping at a Web Speech API value of 4.
      const float kWebSpeechDefault = 1;
      const float kWebSpeechMaxSupported = 4;
      const float kAVSpeechRateHeadroom = AVSpeechUtteranceMaximumSpeechRate -
                                          AVSpeechUtteranceDefaultSpeechRate;
      const float excess = rate - kWebSpeechDefault;
      const float capped_excess =
          std::min(excess, (kWebSpeechMaxSupported - kWebSpeechDefault));
      const float headroom_proportion =
          capped_excess / (kWebSpeechMaxSupported - kWebSpeechDefault);
      rate = AVSpeechUtteranceDefaultSpeechRate +
             headroom_proportion * kAVSpeechRateHeadroom;
    }

    speech_utterance.rate = rate;
  }

  if (params.pitch >= 0.0) {
    speech_utterance.pitchMultiplier = params.pitch;
  }

  if (params.volume >= 0.0) {
    speech_utterance.volume = params.volume;
  }

  [speech_synthesizer_ speakUtterance:speech_utterance];
  std::move(on_speak_finished).Run(true);
}

bool TtsPlatformImplMac::StopSpeaking() {
  [speech_synthesizer_ stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
  paused_ = false;
  return true;
}

void TtsPlatformImplMac::Pause() {
  if (!paused_) {
    [speech_synthesizer_ pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    paused_ = true;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_PAUSE, last_char_index_, kNoLength,
        kNoError);
  }
}

void TtsPlatformImplMac::Resume() {
  if (paused_) {
    [speech_synthesizer_ continueSpeaking];
    paused_ = false;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_RESUME, last_char_index_, kNoLength,
        kNoError);
  }
}

bool TtsPlatformImplMac::IsSpeaking() {
  return speech_synthesizer_.speaking;
}

void TtsPlatformImplMac::GetVoices(std::vector<content::VoiceData>* outVoices) {
  *outVoices = Voices();
}

void TtsPlatformImplMac::OnSpeechEvent(int utterance_id,
                                       content::TtsEventType event_type,
                                       int char_index,
                                       int char_length,
                                       const std::string& error_message) {
  // Don't send events from an utterance that's already completed.
  if (utterance_id != utterance_id_) {
    return;
  }

  if (event_type == content::TTS_EVENT_END) {
    char_index = utterance_.size();
  }

  content::TtsController::GetInstance()->OnTtsEvent(
      utterance_id_, event_type, char_index, char_length, error_message);
  last_char_index_ = char_index;
}

TtsPlatformImplMac::TtsPlatformImplMac()
    : speech_synthesizer_([[AVSpeechSynthesizer alloc] init]),
      delegate_([[ChromeTtsDelegate alloc] initWithPlatformImplMac:this]) {
  speech_synthesizer_.delegate = delegate_;
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
  raw_ptr<TtsPlatformImplMac> _ttsImplMac;  // weak.
}

- (id)initWithPlatformImplMac:(TtsPlatformImplMac*)ttsImplMac {
  if ((self = [super init])) {
    _ttsImplMac = ttsImplMac;
  }
  return self;
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    didStartSpeechUtterance:(AVSpeechUtterance*)utterance {
  _ttsImplMac->OnSpeechEvent(GetUtteranceId(utterance),
                             content::TTS_EVENT_START, /*char_index=*/0,
                             kNoLength, kNoError);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    didFinishSpeechUtterance:(AVSpeechUtterance*)utterance {
  _ttsImplMac->OnSpeechEvent(GetUtteranceId(utterance), content::TTS_EVENT_END,
                             /*char_index=*/0, kNoLength, kNoError);
}

- (void)speechSynthesizer:(AVSpeechSynthesizer*)synthesizer
    willSpeakRangeOfSpeechString:(NSRange)characterRange
                       utterance:(AVSpeechUtterance*)utterance {
  // Ignore bogus ranges. The Mac speech synthesizer is a bit buggy and
  // occasionally returns a number way out of range.
  if (characterRange.location > utterance.speechString.length ||
      characterRange.length == 0) {
    return;
  }
  _ttsImplMac->OnSpeechEvent(GetUtteranceId(utterance), content::TTS_EVENT_WORD,
                             characterRange.location, characterRange.length,
                             kNoError);
}

@end
