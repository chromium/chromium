// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_controller_impl.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/speech/tts_utterance_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "content/public/browser/tts_controller_delegate.h"
#endif

namespace content {
namespace {
// A value to be used to indicate that there is no char index available.
const int kInvalidCharIndex = -1;

// A value to be used to indicate that there is no length available.
const int kInvalidLength = -1;

#if defined(OS_CHROMEOS)
bool VoiceIdMatches(
    const base::Optional<TtsControllerDelegate::PreferredVoiceId>& id,
    const content::VoiceData& voice) {
  if (!id.has_value() || voice.name.empty() ||
      (voice.engine_id.empty() && !voice.native))
    return false;
  if (voice.native)
    return id->name == voice.name && id->id.empty();
  return id->name == voice.name && id->id == voice.engine_id;
}
#endif  // defined(OS_CHROMEOS)

TtsUtteranceImpl* AsUtteranceImpl(TtsUtterance* utterance) {
  return static_cast<TtsUtteranceImpl*>(utterance);
}

}  // namespace

//
// VoiceData
//

VoiceData::VoiceData() : remote(false), native(false) {}

VoiceData::VoiceData(const VoiceData& other) = default;

VoiceData::~VoiceData() {}

//
// TtsController
//

TtsController* TtsController::GetInstance() {
  return TtsControllerImpl::GetInstance();
}

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
enum class UMATextToSpeechEvent {
  START = 0,
  END = 1,
  WORD = 2,
  SENTENCE = 3,
  MARKER = 4,
  INTERRUPTED = 5,
  CANCELLED = 6,
  SPEECH_ERROR = 7,
  PAUSE = 8,
  RESUME = 9,

  // This must always be the last enum. It's okay for its value to
  // increase, but none of the other enum values may change.
  COUNT
};

//
// TtsControllerImpl
//

// static
TtsControllerImpl* TtsControllerImpl::GetInstance() {
  return base::Singleton<TtsControllerImpl>::get();
}

void TtsControllerImpl::SetStopSpeakingWhenHidden(bool value) {
  stop_speaking_when_hidden_ = value;
}

TtsControllerImpl::TtsControllerImpl() = default;

TtsControllerImpl::~TtsControllerImpl() {
  if (current_utterance_) {
    current_utterance_->Finish();
    SetCurrentUtterance(nullptr);
  }

  // Clear any queued utterances too.
  ClearUtteranceQueue(false);  // Don't sent events.
}

void TtsControllerImpl::SpeakOrEnqueue(
    std::unique_ptr<TtsUtterance> utterance) {
  if (!ShouldSpeakUtterance(utterance.get())) {
    utterance->Finish();
    return;
  }

  // If we're paused and we get an utterance that can't be queued,
  // flush the queue but stay in the paused state.
  if (paused_ && !utterance->GetCanEnqueue()) {
    utterance_list_.emplace_back(std::move(utterance));
    Stop();
    paused_ = true;
    return;
  }

  if (paused_ || (IsSpeaking() && utterance->GetCanEnqueue())) {
    utterance_list_.emplace_back(std::move(utterance));
  } else {
    Stop();
    SpeakNow(std::move(utterance));
  }
}

void TtsControllerImpl::Stop() {
  StopAndClearQueue(GURL());
}

void TtsControllerImpl::Stop(const GURL& source_url) {
  StopAndClearQueue(source_url);
}

void TtsControllerImpl::StopAndClearQueue(const GURL& source_url) {
  if (StopCurrentUtteranceIfMatches(source_url))
    ClearUtteranceQueue(true);
}

bool TtsControllerImpl::StopCurrentUtteranceIfMatches(const GURL& source_url) {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Stop"));

  paused_ = false;

  if (!source_url.is_empty() && current_utterance_ &&
      current_utterance_->GetSrcUrl().GetOrigin() != source_url.GetOrigin())
    return false;

  if (current_utterance_ && !current_utterance_->GetEngineId().empty()) {
    if (engine_delegate_)
      engine_delegate_->Stop(current_utterance_.get());
  } else if (GetTtsPlatform()->PlatformImplAvailable()) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->StopSpeaking();
  }

  if (current_utterance_)
    current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                   kInvalidLength, std::string());
  FinishCurrentUtterance();
  return true;
}

void TtsControllerImpl::Pause() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Pause"));

  paused_ = true;
  if (current_utterance_ && !current_utterance_->GetEngineId().empty()) {
    if (engine_delegate_)
      engine_delegate_->Pause(current_utterance_.get());
  } else if (current_utterance_) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Pause();
  }
}

void TtsControllerImpl::Resume() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Resume"));

  paused_ = false;
  if (current_utterance_ && !current_utterance_->GetEngineId().empty()) {
    if (engine_delegate_)
      engine_delegate_->Resume(current_utterance_.get());
  } else if (current_utterance_) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Resume();
  } else {
    SpeakNextUtterance();
  }
}

void TtsControllerImpl::OnTtsEvent(int utterance_id,
                                   TtsEventType event_type,
                                   int char_index,
                                   int length,
                                   const std::string& error_message) {
  // We may sometimes receive completion callbacks "late", after we've
  // already finished the utterance (for example because another utterance
  // interrupted or we got a call to Stop). This is normal and we can
  // safely just ignore these events.
  if (!current_utterance_ || utterance_id != current_utterance_->GetId()) {
    return;
  }

  UMATextToSpeechEvent metric;
  switch (event_type) {
    case TTS_EVENT_START:
      metric = UMATextToSpeechEvent::START;
      break;
    case TTS_EVENT_END:
      metric = UMATextToSpeechEvent::END;
      break;
    case TTS_EVENT_WORD:
      metric = UMATextToSpeechEvent::WORD;
      break;
    case TTS_EVENT_SENTENCE:
      metric = UMATextToSpeechEvent::SENTENCE;
      break;
    case TTS_EVENT_MARKER:
      metric = UMATextToSpeechEvent::MARKER;
      break;
    case TTS_EVENT_INTERRUPTED:
      metric = UMATextToSpeechEvent::INTERRUPTED;
      break;
    case TTS_EVENT_CANCELLED:
      metric = UMATextToSpeechEvent::CANCELLED;
      break;
    case TTS_EVENT_ERROR:
      metric = UMATextToSpeechEvent::SPEECH_ERROR;
      break;
    case TTS_EVENT_PAUSE:
      metric = UMATextToSpeechEvent::PAUSE;
      break;
    case TTS_EVENT_RESUME:
      metric = UMATextToSpeechEvent::RESUME;
      break;
    default:
      NOTREACHED();
      return;
  }
  UMA_HISTOGRAM_ENUMERATION("TextToSpeech.Event", metric,
                            UMATextToSpeechEvent::COUNT);

  current_utterance_->OnTtsEvent(event_type, char_index, length, error_message);
  if (current_utterance_->IsFinished()) {
    FinishCurrentUtterance();
    SpeakNextUtterance();
  }
}

void TtsControllerImpl::GetVoices(BrowserContext* browser_context,
                                  std::vector<VoiceData>* out_voices) {
  TtsPlatform* tts_platform = GetTtsPlatform();
  DCHECK(tts_platform);
  // Ensure we have all built-in voices loaded. This is a no-op if already
  // loaded.
  tts_platform->LoadBuiltInTtsEngine(browser_context);
  if (tts_platform->PlatformImplAvailable())
    tts_platform->GetVoices(out_voices);

  if (browser_context && engine_delegate_)
    engine_delegate_->GetVoices(browser_context, out_voices);
}

bool TtsControllerImpl::IsSpeaking() {
  return current_utterance_ != nullptr || GetTtsPlatform()->IsSpeaking();
}

void TtsControllerImpl::VoicesChanged() {
  // Existence of platform tts indicates explicit requests to tts. Since
  // |VoicesChanged| can occur implicitly, only send if needed.
  for (auto& delegate : voices_changed_delegates_)
    delegate.OnVoicesChanged();
}

void TtsControllerImpl::AddVoicesChangedDelegate(
    VoicesChangedDelegate* delegate) {
  voices_changed_delegates_.AddObserver(delegate);
}

void TtsControllerImpl::RemoveVoicesChangedDelegate(
    VoicesChangedDelegate* delegate) {
  voices_changed_delegates_.RemoveObserver(delegate);
}

void TtsControllerImpl::RemoveUtteranceEventDelegate(
    UtteranceEventDelegate* delegate) {
  // First clear any pending utterances with this delegate.
  std::list<std::unique_ptr<TtsUtterance>> old_list;
  utterance_list_.swap(old_list);
  while (!old_list.empty()) {
    std::unique_ptr<TtsUtterance> utterance = std::move(old_list.front());
    old_list.pop_front();
    if (utterance->GetEventDelegate() != delegate)
      utterance_list_.emplace_back(std::move(utterance));
  }

  if (current_utterance_ &&
      current_utterance_->GetEventDelegate() == delegate) {
    current_utterance_->SetEventDelegate(nullptr);
    if (!current_utterance_->GetEngineId().empty()) {
      if (engine_delegate_)
        engine_delegate_->Stop(current_utterance_.get());
    } else {
      GetTtsPlatform()->ClearError();
      GetTtsPlatform()->StopSpeaking();
    }

    FinishCurrentUtterance();
    if (!paused_)
      SpeakNextUtterance();
  }
}

void TtsControllerImpl::SetTtsEngineDelegate(TtsEngineDelegate* delegate) {
  engine_delegate_ = delegate;
}

TtsEngineDelegate* TtsControllerImpl::GetTtsEngineDelegate() {
  return engine_delegate_;
}

void TtsControllerImpl::OnBrowserContextDestroyed(
    BrowserContext* browser_context) {
  bool did_clear_utterances = false;

  // First clear the BrowserContext from any utterances.
  for (std::unique_ptr<TtsUtterance>& utterance : utterance_list_) {
    if (utterance->GetBrowserContext() == browser_context) {
      utterance->ClearBrowserContext();
      did_clear_utterances = true;
    }
  }

  if (current_utterance_ &&
      current_utterance_->GetBrowserContext() == browser_context) {
    current_utterance_->ClearBrowserContext();
    did_clear_utterances = true;
  }

  // If we cleared the BrowserContext from any utterances, stop speech
  // just to be safe. Do this using PostTask because calling Stop might
  // try to send notifications and that can trigger code paths that try
  // to access the BrowserContext that's being deleted. Note that it's
  // safe to use base::Unretained because this is a singleton.
  if (did_clear_utterances) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&TtsControllerImpl::StopAndClearQueue,
                                  base::Unretained(this), GURL()));
  }
}

void TtsControllerImpl::SetTtsPlatform(TtsPlatform* tts_platform) {
  tts_platform_ = tts_platform;
}

int TtsControllerImpl::QueueSize() {
  return static_cast<int>(utterance_list_.size());
}

TtsPlatform* TtsControllerImpl::GetTtsPlatform() {
  if (!tts_platform_)
    tts_platform_ = TtsPlatform::GetInstance();
  return tts_platform_;
}

void TtsControllerImpl::SpeakNow(std::unique_ptr<TtsUtterance> utterance) {
  // Get all available voices and try to find a matching voice.
  std::vector<VoiceData> voices;
  GetVoices(utterance->GetBrowserContext(), &voices);

  // Get the best matching voice. If nothing matches, just set "native"
  // to true because that might trigger deferred loading of native voices.
  // TODO(katie): Move most of the GetMatchingVoice logic into content/ and
  // use the TTS controller delegate to get chrome-specific info as needed.
  int index = GetMatchingVoice(utterance.get(), voices);
  VoiceData voice;
  if (index >= 0)
    voice = voices[index];
  else
    voice.native = true;

  UpdateUtteranceDefaults(utterance.get());

  GetTtsPlatform()->WillSpeakUtteranceWithVoice(utterance.get(), voice);

  base::RecordAction(base::UserMetricsAction("TextToSpeech.Speak"));
  UMA_HISTOGRAM_COUNTS_100000("TextToSpeech.Utterance.TextLength",
                              utterance->GetText().size());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.FromExtensionAPI",
                        !utterance->GetSrcUrl().is_empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasVoiceName",
                        !utterance->GetVoiceName().empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasLang",
                        !utterance->GetLang().empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasRate",
                        utterance->GetContinuousParameters().rate != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasPitch",
                        utterance->GetContinuousParameters().pitch != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasVolume",
                        utterance->GetContinuousParameters().volume != 1.0);
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.Native", voice.native);

  if (!voice.native) {
#if !defined(OS_ANDROID)
    DCHECK(!voice.engine_id.empty());
    SetCurrentUtterance(std::move(utterance));
    current_utterance_->SetEngineId(voice.engine_id);
    if (engine_delegate_)
      engine_delegate_->Speak(current_utterance_.get(), voice);
    bool sends_end_event =
        voice.events.find(TTS_EVENT_END) != voice.events.end();
    if (!sends_end_event) {
      current_utterance_->Finish();
      SetCurrentUtterance(nullptr);
      SpeakNextUtterance();
    }
#endif  // !defined(OS_ANDROID)
  } else {
    // It's possible for certain platforms to send start events immediately
    // during |speak|.
    SetCurrentUtterance(std::move(utterance));
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Speak(
        current_utterance_->GetId(), current_utterance_->GetText(),
        current_utterance_->GetLang(), voice,
        current_utterance_->GetContinuousParameters(),
        base::BindOnce(&TtsControllerImpl::OnSpeakFinished,
                       base::Unretained(this), current_utterance_->GetId()));
  }
}

void TtsControllerImpl::OnSpeakFinished(int utterance_id, bool success) {
  if (success)
    return;

  // Since OnSpeakFinished could run asynchronously, it is possible that the
  // current utterance has changed. Ignore any such spurious callbacks.
  if (!current_utterance_ || current_utterance_->GetId() != utterance_id)
    return;

  // If the native voice wasn't able to process this speech, see if
  // the browser has built-in TTS that isn't loaded yet.
  if (GetTtsPlatform()->LoadBuiltInTtsEngine(
          current_utterance_->GetBrowserContext())) {
    utterance_list_.emplace_back(std::move(current_utterance_));
    return;
  }

  current_utterance_->OnTtsEvent(TTS_EVENT_ERROR, kInvalidCharIndex,
                                 kInvalidLength, GetTtsPlatform()->GetError());
  SetCurrentUtterance(nullptr);
}

void TtsControllerImpl::ClearUtteranceQueue(bool send_events) {
  while (!utterance_list_.empty()) {
    std::unique_ptr<TtsUtterance> utterance =
        std::move(utterance_list_.front());
    utterance_list_.pop_front();
    if (send_events) {
      utterance->OnTtsEvent(TTS_EVENT_CANCELLED, kInvalidCharIndex,
                            kInvalidLength, std::string());
    } else {
      utterance->Finish();
    }
  }
}

void TtsControllerImpl::FinishCurrentUtterance() {
  if (current_utterance_) {
    if (!current_utterance_->IsFinished())
      current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                     kInvalidLength, std::string());
    SetCurrentUtterance(nullptr);
  }
}

void TtsControllerImpl::SpeakNextUtterance() {
  if (paused_)
    return;

  // Start speaking the next utterance in the queue.  Keep trying in case
  // one fails but there are still more in the queue to try.
  while (!utterance_list_.empty() && !current_utterance_) {
    std::unique_ptr<TtsUtterance> utterance =
        std::move(utterance_list_.front());
    utterance_list_.pop_front();
    if (ShouldSpeakUtterance(utterance.get()))
      SpeakNow(std::move(utterance));
    else
      utterance->Finish();
  }
}

void TtsControllerImpl::UpdateUtteranceDefaults(TtsUtterance* utterance) {
  double rate = utterance->GetContinuousParameters().rate;
  double pitch = utterance->GetContinuousParameters().pitch;
  double volume = utterance->GetContinuousParameters().volume;
#if defined(OS_CHROMEOS)
  if (GetTtsControllerDelegate())
    GetTtsControllerDelegate()->UpdateUtteranceDefaultsFromPrefs(
        utterance, &rate, &pitch, &volume);
#else
  // Update pitch, rate and volume to defaults if not explicity set on
  // this utterance.
  if (rate == blink::mojom::kSpeechSynthesisDoublePrefNotSet)
    rate = blink::mojom::kSpeechSynthesisDefaultRate;
  if (pitch == blink::mojom::kSpeechSynthesisDoublePrefNotSet)
    pitch = blink::mojom::kSpeechSynthesisDefaultPitch;
  if (volume == blink::mojom::kSpeechSynthesisDoublePrefNotSet)
    volume = blink::mojom::kSpeechSynthesisDefaultVolume;
#endif  // defined(OS_CHROMEOS)
  utterance->SetContinuousParameters(rate, pitch, volume);
}

void TtsControllerImpl::StripSSML(
    const std::string& utterance,
    base::OnceCallback<void(const std::string&)> on_ssml_parsed) {
  // Skip parsing and return if not xml.
  if (utterance.find("<?xml") == std::string::npos) {
    std::move(on_ssml_parsed).Run(utterance);
    return;
  }

  // Parse using safe, out-of-process Xml Parser.
  data_decoder::DataDecoder::ParseXmlIsolated(
      utterance, base::BindOnce(&TtsControllerImpl::StripSSMLHelper, utterance,
                                std::move(on_ssml_parsed)));
}

// Called when ParseXml finishes.
// Uses parsed xml to build parsed utterance text.
void TtsControllerImpl::StripSSMLHelper(
    const std::string& utterance,
    base::OnceCallback<void(const std::string&)> on_ssml_parsed,
    data_decoder::DataDecoder::ValueOrError result) {
  // Error checks.
  // If invalid xml, return original utterance text.
  if (!result.value) {
    std::move(on_ssml_parsed).Run(utterance);
    return;
  }

  std::string root_tag_name;
  data_decoder::GetXmlElementTagName(*result.value, &root_tag_name);
  // Root element must be <speak>.
  if (root_tag_name.compare("speak") != 0) {
    std::move(on_ssml_parsed).Run(utterance);
    return;
  }

  std::string parsed_text;
  // Change from unique_ptr to base::Value* so recursion will work.
  PopulateParsedText(&parsed_text, &(*result.value));

  // Run with parsed_text.
  std::move(on_ssml_parsed).Run(parsed_text);
}

void TtsControllerImpl::PopulateParsedText(std::string* parsed_text,
                                           const base::Value* element) {
  DCHECK(parsed_text);
  if (!element)
    return;
  // Add element's text if present.
  // Note: We don't use data_decoder::GetXmlElementText because it gets the text
  // of element's first child, not text of current element.
  const base::Value* text_value = element->FindKeyOfType(
      data_decoder::mojom::XmlParser::kTextKey, base::Value::Type::STRING);
  if (text_value)
    *parsed_text += text_value->GetString();

  const base::Value* children = data_decoder::GetXmlElementChildren(*element);
  if (!children || !children->is_list())
    return;

  for (size_t i = 0; i < children->GetList().size(); ++i) {
    // We need to iterate over all children because some text elements are
    // nested within other types of elements, such as <emphasis> tags.
    PopulateParsedText(parsed_text, &children->GetList()[i]);
  }
}

int TtsControllerImpl::GetMatchingVoice(TtsUtterance* utterance,
                                        const std::vector<VoiceData>& voices) {
  const std::string app_lang =
      GetContentClient()->browser()->GetApplicationLocale();
  // Start with a best score of -1, that way even if none of the criteria
  // match, something will be returned if there are any voices.
  int best_score = -1;
  int best_score_index = -1;
#if defined(OS_CHROMEOS)
  TtsControllerDelegate* delegate = GetTtsControllerDelegate();
  std::unique_ptr<TtsControllerDelegate::PreferredVoiceIds> preferred_ids =
      delegate ? delegate->GetPreferredVoiceIdsForUtterance(utterance)
               : nullptr;
#endif  // defined(OS_CHROMEOS)
  for (size_t i = 0; i < voices.size(); ++i) {
    const content::VoiceData& voice = voices[i];
    int score = 0;

    // If the extension ID is specified, check for an exact match.
    if (!utterance->GetEngineId().empty() &&
        utterance->GetEngineId() != voice.engine_id)
      continue;

    // If the voice name is specified, check for an exact match.
    if (!utterance->GetVoiceName().empty() &&
        voice.name != utterance->GetVoiceName())
      continue;

    // Prefer the utterance language.
    if (!voice.lang.empty() && !utterance->GetLang().empty()) {
      // An exact language match is worth more than a partial match.
      if (voice.lang == utterance->GetLang()) {
        score += 128;
      } else if (l10n_util::GetLanguage(voice.lang) ==
                 l10n_util::GetLanguage(utterance->GetLang())) {
        score += 64;
      }
    }

    // Next, prefer required event types.
    if (!utterance->GetRequiredEventTypes().empty()) {
      bool has_all_required_event_types = true;
      for (TtsEventType event_type : utterance->GetRequiredEventTypes()) {
        if (voice.events.find(event_type) == voice.events.end()) {
          has_all_required_event_types = false;
          break;
        }
      }
      if (has_all_required_event_types)
        score += 32;
    }

#if defined(OS_CHROMEOS)
    if (preferred_ids) {
      // First prefer the user's preference voice for the utterance language,
      // if the utterance language is specified.
      if (!utterance->GetLang().empty() &&
          VoiceIdMatches(preferred_ids->lang_voice_id, voice)) {
        score += 16;
      }

      // Then prefer the user's preference voice for the system language.
      // This is a lower priority match than the utterance voice.
      if (VoiceIdMatches(preferred_ids->locale_voice_id, voice))
        score += 8;

      // Finally, prefer the user's preference voice for any language. This will
      // pick the default voice if there is no better match for the current
      // system language and utterance language.
      if (VoiceIdMatches(preferred_ids->any_locale_voice_id, voice))
        score += 4;
    }
#endif  // defined(OS_CHROMEOS)

    // Finally, prefer system language.
    if (!voice.lang.empty()) {
      if (voice.lang == app_lang) {
        score += 2;
      } else if (l10n_util::GetLanguage(voice.lang) ==
                 l10n_util::GetLanguage(app_lang)) {
        score += 1;
      }
    }

    if (score > best_score) {
      best_score = score;
      best_score_index = i;
    }
  }

  return best_score_index;
}

void TtsControllerImpl::SetCurrentUtterance(
    std::unique_ptr<TtsUtterance> utterance) {
  current_utterance_ = std::move(utterance);
  Observe(current_utterance_
              ? AsUtteranceImpl(current_utterance_.get())->web_contents()
              : nullptr);
}

void TtsControllerImpl::StopCurrentUtteranceAndRemoveUtterancesMatching(
    WebContents* wc) {
  DCHECK(wc);
  // Removes any utterances that match the WebContents from the current
  // utterance (which our inherited WebContentsObserver starts observing every
  // time the utterance changes).
  //
  // This is called when the WebContents for the current utterance is destroyed
  // or hidden. In the case where it's destroyed, this is done to avoid
  // attempting to start a utterance that is very likely to be destroyed right
  // away, and there are also subtle timing issues if we didn't do this (if a
  // queued utterance has already received WebContentsDestroyed(), and we start
  // it, we won't get the corresponding WebContentsDestroyed()).
  auto eraser = [wc](const std::unique_ptr<TtsUtterance>& utterance) {
    TtsUtteranceImpl* utterance_impl = AsUtteranceImpl(utterance.get());
    if (utterance_impl->web_contents() == wc) {
      utterance_impl->Finish();
      return true;
    }
    return false;
  };
  utterance_list_.erase(
      std::remove_if(utterance_list_.begin(), utterance_list_.end(), eraser),
      utterance_list_.end());
  const bool stopped = StopCurrentUtteranceIfMatches(GURL());
  DCHECK(stopped);
  SpeakNextUtterance();
}

bool TtsControllerImpl::ShouldSpeakUtterance(TtsUtterance* utterance) {
  TtsUtteranceImpl* utterance_impl = AsUtteranceImpl(utterance);
  if (!utterance_impl->was_created_with_web_contents())
    return true;

  // If the WebContents that created the utterance has been destroyed, don't
  // speak it.
  if (!utterance_impl->web_contents())
    return false;

  // Allow speaking if either the WebContents is visible, or the WebContents
  // isn't required to be visible before speaking.
  return !stop_speaking_when_hidden_ ||
         utterance_impl->web_contents()->GetVisibility() != Visibility::HIDDEN;
}

//
// WebContentsObserver
//

void TtsControllerImpl::WebContentsDestroyed() {
  StopCurrentUtteranceAndRemoveUtterancesMatching(web_contents());
}

void TtsControllerImpl::OnVisibilityChanged(Visibility visibility) {
  if (visibility == Visibility::HIDDEN && stop_speaking_when_hidden_)
    StopCurrentUtteranceAndRemoveUtterancesMatching(web_contents());
}

#if defined(OS_CHROMEOS)
TtsControllerDelegate* TtsControllerImpl::GetTtsControllerDelegate() {
  if (delegate_)
    return delegate_;
  if (GetContentClient() && GetContentClient()->browser()) {
    delegate_ = GetContentClient()->browser()->GetTtsControllerDelegate();
    return delegate_;
  }
  return nullptr;
}

void TtsControllerImpl::SetTtsControllerDelegateForTesting(
    TtsControllerDelegate* delegate) {
  delegate_ = delegate;
}

#endif  // defined(OS_CHROMEOS)

}  // namespace content
