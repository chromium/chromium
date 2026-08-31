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
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "content/public/browser/tts_controller.h"

namespace {

constexpr int kNoLength = -1;
constexpr char kNoError[] = "";

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

AVSpeechSynthesisVoice*
TtsPlatformImplMacBackgroundWorker::GetSystemDefaultVoice() {
  // This should be
  //
  //   [AVSpeechSynthesisVoice voiceWithLanguage:nil]
  //
  // but that has a bug (https://crbug.com/1484940#c9, FB13197951). In short,
  // while passing nil to -[AVSpeechSynthesisVoice voiceWithLanguage:] does
  // indeed return "the default voice for the system’s language and region",
  // that's not necessarily the voice that the user selected in System Settings
  // > Accessibility > Spoken Content, and that user voice selection is the only
  // one that matters. The first two workarounds below behave correctly.
  NSUserDefaults* accessibility_defaults =
      [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.Accessibility"];

  // SpokenContentDefaultVoiceSelectionsByLanguage is an array that maps a
  // language code to a dictionary of voice selection details.
  //
  // SpokenContentDefaultVoiceSelectionsByLanguage Structure:
  // @[
  //   @"en", // System language code (NSString).
  //   @{
  //     @"_type": @"Speech.VoiceSelection",  // Type identifier (NSString).
  //     @"_version": @0,  // Voice format version (NSNumber).
  //     @"boundLanguage": @"en",  // Language bound to this voice (NSString).
  //     @"voiceId":
  //         @"com.apple.voice.compact.en-IE.Moira" // Unique ID (NSString).
  //   }
  // ]
  NSArray* spoken_default_voice_settings = [accessibility_defaults
      arrayForKey:@"SpokenContentDefaultVoiceSelectionsByLanguage"];

  AVSpeechSynthesisVoice* voice = nil;

  // Attempt 1: Get the user-selected voice from accessibility defaults.
  //
  // Process `spoken_default_voice_settings` only if the voice selection data
  // (expected second value in SpokenContentDefaultVoiceSelectionsByLanguage) is
  // present. This is a precautionary check.
  if (spoken_default_voice_settings.count > 1) {
    NSDictionary* selected_voice_settings = spoken_default_voice_settings[1];
    NSString* selected_voice_id = selected_voice_settings[@"voiceId"];
    voice = [AVSpeechSynthesisVoice voiceWithIdentifier:selected_voice_id];
  }

  // Attempt 2: Get the user-selected voice from the deprecated
  // NSSpeechSynthesizer API.
  if (!voice) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  NSString* default_voice_identifier = NSSpeechSynthesizer.defaultVoice;
#pragma clang diagnostic pop
  voice = [AVSpeechSynthesisVoice voiceWithIdentifier:default_voice_identifier];
  }

  // Fallback to the default voice for the system's language and location if we
  // are unable to get the user-selected voice. This is the next most-specific
  // voice preference that we are able to retrieve using supported APIs.
  if (!voice) {
    return [AVSpeechSynthesisVoice voiceWithLanguage:nil];
  }

  return voice;
}

std::string
TtsPlatformImplMacBackgroundWorker::GetSystemDefaultVoiceIdentifier() {
  AVSpeechSynthesisVoice* default_voice = GetSystemDefaultVoice();
  return default_voice ? base::SysNSStringToUTF8(default_voice.identifier)
                       : std::string();
}

TtsPlatformImplMacBackgroundWorker::Voices::Voices() = default;
TtsPlatformImplMacBackgroundWorker::Voices::Voices(Voices&&) = default;
TtsPlatformImplMacBackgroundWorker::Voices&
TtsPlatformImplMacBackgroundWorker::Voices::operator=(Voices&&) = default;
TtsPlatformImplMacBackgroundWorker::Voices::~Voices() = default;

TtsPlatformImplMacBackgroundWorker::Voices
TtsPlatformImplMacBackgroundWorker::LoadVoices() {
  Voices result;

  AVSpeechSynthesisVoice* default_voice = GetSystemDefaultVoice();
  if (default_voice) {
    result.default_voice_identifier =
        base::SysNSStringToUTF8(default_voice.identifier);
  }

  NSMutableArray* av_speech_voices =
      [[AVSpeechSynthesisVoice.speechVoices sortedArrayUsingDescriptors:@[
        [NSSortDescriptor sortDescriptorWithKey:@"name" ascending:YES]
      ]] mutableCopy];
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

  result.voices.reserve(av_speech_voices.count);
  for (AVSpeechSynthesisVoice* av_speech_voice in av_speech_voices) {
    NSString* voice_name = av_speech_voice.name;
    if (!voice_name) {
      // AVSpeechSynthesisVoice.name is not a nullable property, but there are
      // crashes (https://crbug.com/1459235) where it seems like it's returning
      // nil. Without a name, a voice is useless, so skip it.
      continue;
    }

    result.voices.emplace_back();
    content::VoiceData& data = result.voices.back();

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

  return result;
}

// static
content::TtsPlatformImpl* content::TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplMac::GetInstance();
}

TtsPlatformImplMac::~TtsPlatformImplMac() {
  if (application_active_observer_) {
    [NSNotificationCenter.defaultCenter
        removeObserver:application_active_observer_];
  }
}

bool TtsPlatformImplMac::PlatformImplSupported() {
  return true;
}

bool TtsPlatformImplMac::PlatformImplInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return voices_loaded_;
}

void TtsPlatformImplMac::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const content::VoiceData& voice,
    const content::UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  received_voices_request_ = true;
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [speech_synthesizer_ stopSpeakingAtBoundary:AVSpeechBoundaryImmediate];
  paused_ = false;
  return true;
}

void TtsPlatformImplMac::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!paused_) {
    [speech_synthesizer_ pauseSpeakingAtBoundary:AVSpeechBoundaryImmediate];
    paused_ = true;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_PAUSE, last_char_index_, kNoLength,
        kNoError);
  }
}

void TtsPlatformImplMac::Resume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (paused_) {
    [speech_synthesizer_ continueSpeaking];
    paused_ = false;
    content::TtsController::GetInstance()->OnTtsEvent(
        utterance_id_, content::TTS_EVENT_RESUME, last_char_index_, kNoLength,
        kNoError);
  }
}

bool TtsPlatformImplMac::IsSpeaking() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return speech_synthesizer_.speaking;
}

void TtsPlatformImplMac::GetVoices(std::vector<content::VoiceData>* outVoices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  received_voices_request_ = true;
  *outVoices = voices_;
}

void TtsPlatformImplMac::RefreshVoices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  received_voices_request_ = true;
  LoadVoices();
}

void TtsPlatformImplMac::LoadVoices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_loading_voices_) {
    needs_reload_voices_ = true;
    return;
  }
  is_loading_voices_ = true;

  GetBackgroundWorker()
      .AsyncCall(&TtsPlatformImplMacBackgroundWorker::LoadVoices)
      .Then(base::BindOnce(&TtsPlatformImplMac::OnVoicesLoaded,
                           base::Unretained(this)));
}

void TtsPlatformImplMac::OnVoicesLoaded(
    TtsPlatformImplMacBackgroundWorker::Voices voices) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_loading_voices_ = false;
  default_voice_identifier_ = std::move(voices.default_voice_identifier);
  voices_ = std::move(voices.voices);
  voices_loaded_ = true;

  // Tells pages (voiceschanged) and other delegates about the new list, and
  // speaks anything TtsController queued while the first load was pending.
  content::TtsController::GetInstance()->VoicesChanged();

  if (needs_reload_voices_) {
    needs_reload_voices_ = false;
    LoadVoices();
  }
}

void TtsPlatformImplMac::UpdateSystemDefaultVoice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // A load in flight fetches the default voice anyway.
  if (is_loading_voices_ || is_updating_default_voice_) {
    return;
  }
  is_updating_default_voice_ = true;

  GetBackgroundWorker()
      .AsyncCall(
          &TtsPlatformImplMacBackgroundWorker::GetSystemDefaultVoiceIdentifier)
      .Then(base::BindOnce(&TtsPlatformImplMac::OnGotDefaultVoiceIdentifier,
                           base::Unretained(this)));
}

void TtsPlatformImplMac::OnGotDefaultVoiceIdentifier(
    std::string default_voice_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_updating_default_voice_ = false;
  if (default_voice_identifier != default_voice_identifier_) {
    LoadVoices();
  }
}

void TtsPlatformImplMac::OnApplicationWillBecomeActive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!received_voices_request_) {
    return;
  }
  UpdateSystemDefaultVoice();
}

void TtsPlatformImplMac::OnSpeechEvent(int utterance_id,
                                       content::TtsEventType event_type,
                                       int char_index,
                                       int char_length,
                                       const std::string& error_message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  application_active_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:NSApplicationWillBecomeActiveNotification
                  object:nil
                   queue:NSOperationQueue.mainQueue
              usingBlock:^(NSNotification* notification) {
                // The user might have switched to Settings or some other app
                // to change the default voice. Check for that when the app
                // becomes active again and rebuild the voices vector if so.
                TtsPlatformImplMac::GetInstance()
                    ->OnApplicationWillBecomeActive();
              }];
  LoadVoices();
}

// static
TtsPlatformImplMac* TtsPlatformImplMac::GetInstance() {
  static base::NoDestructor<TtsPlatformImplMac> tts_platform;
  return tts_platform.get();
}

// static
base::SequenceBound<TtsPlatformImplMacBackgroundWorker>&
TtsPlatformImplMac::GetBackgroundWorker() {
  static base::NoDestructor<
      base::SequenceBound<TtsPlatformImplMacBackgroundWorker>>
      worker(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE}));
  return *worker;
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
