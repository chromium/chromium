// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_controller_impl.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/speech/tts_utterance_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/tts_utterance.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "services/data_decoder/public/cpp/safe_xml_parser.h"
#include "services/data_decoder/public/mojom/xml_parser.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/public/browser/tts_controller_delegate.h"
#endif

namespace content {
namespace {
// A value to be used to indicate that there is no char index available.
const int kInvalidCharIndex = -1;

// A value to be used to indicate that there is no length available.
const int kInvalidLength = -1;

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool VoiceIdMatches(
    const std::optional<TtsControllerDelegate::PreferredVoiceId>& id,
    const content::VoiceData& voice) {
  if (!id.has_value() || voice.name.empty() ||
      (voice.engine_id.empty() && !voice.native))
    return false;
  if (voice.native)
    return id->name == voice.name && id->id.empty();
  return id->name == voice.name && id->id == voice.engine_id;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TtsUtteranceImpl* AsUtteranceImpl(TtsUtterance* utterance) {
  return static_cast<TtsUtteranceImpl*>(utterance);
}

bool IsUtteranceSpokenByRemoteEngine(TtsUtterance* utterance) {
  if (utterance && !utterance->GetEngineId().empty()) {
    TtsUtteranceImpl* utterance_impl = AsUtteranceImpl(utterance);
    return utterance_impl->spoken_by_remote_engine();
  }
  return false;
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

void TtsController::SkipAddNetworkChangeObserverForTests(bool enabled) {
  return TtsControllerImpl::SkipAddNetworkChangeObserverForTests(enabled);
}

// IMPORTANT!
// These values are written to logs.  Do not renumber or delete
// existing items; add new entries to the end of the list.
// LINT.IfChange(UMATextToSpeechEvent)
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
// LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:TextToSpeechEvent)

//
// TtsControllerImpl
//

// static
bool TtsControllerImpl::skip_add_network_change_observer_for_tests_ = false;

// static
TtsControllerImpl* TtsControllerImpl::GetInstance() {
  return base::Singleton<TtsControllerImpl>::get();
}

// static
void TtsControllerImpl::SkipAddNetworkChangeObserverForTests(bool enabled) {
  TtsControllerImpl::skip_add_network_change_observer_for_tests_ = enabled;
}

void TtsControllerImpl::SetStopSpeakingWhenHidden(bool value) {
  stop_speaking_when_hidden_ = value;
}

TtsControllerImpl::TtsControllerImpl() {
  if (!skip_add_network_change_observer_for_tests_) {
    net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
  }
  OnNetworkChanged(net::NetworkChangeNotifier::GetConnectionType());
}

TtsControllerImpl::~TtsControllerImpl() {
  if (current_utterance_) {
    current_utterance_->Finish();
    SetCurrentUtterance(nullptr);
  }

  // Clear any queued utterances too.
  ClearUtteranceQueue(false);  // Don't sent events.

  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void TtsControllerImpl::SpeakOrEnqueue(
    std::unique_ptr<TtsUtterance> utterance) {
  auto* external_delegate = GetTtsPlatform()->GetExternalPlatformDelegate();
  if (external_delegate) {
    GetTtsPlatform()->GetExternalPlatformDelegate()->Enqueue(
        std::move(utterance));
    return;
  }

  SpeakOrEnqueueInternal(std::move(utterance));
}

void TtsControllerImpl::SpeakOrEnqueueInternal(
    std::unique_ptr<TtsUtterance> utterance) {
  if (!ShouldSpeakUtterance(utterance.get())) {
    utterance->Finish();
    return;
  }

  // If the TTS platform or tts engine delegate is still loading or
  // initializing, queue or flush the utterance. The utterances can be sent to
  // platform specific implementation or to the engine implementation. Every
  // utterances are postponed until the platform specific implementation and
  // built in tts engine are loaded to avoid races where the utterance gets
  // dropped unexpectedly.
  if (TtsPlatformLoading() ||
      (engine_delegate_ && !engine_delegate_->IsBuiltInTtsEngineInitialized(
                               utterance->GetBrowserContext()))) {
    GetTtsPlatform()->LoadBuiltInTtsEngine(utterance->GetBrowserContext());

    if (utterance->GetShouldClearQueue())
      ClearUtteranceQueue(true);

    utterance_list_.emplace_back(std::move(utterance));
    return;
  }

  // If we're paused and we get an utterance that can't be queued,
  // flush the queue but stay in the paused state.
  if (paused_ && utterance->GetShouldClearQueue()) {
    Stop();
    utterance_list_.emplace_back(std::move(utterance));
    paused_ = true;
    return;
  }

  if (paused_ || (IsSpeaking() && !utterance->GetShouldClearQueue())) {
    utterance_list_.emplace_back(std::move(utterance));
  } else {
    Stop();
    SpeakNow(std::move(utterance));
  }
}

void TtsControllerImpl::Stop() {
  Stop(GURL());
}

void TtsControllerImpl::Stop(const GURL& source_url) {
  auto* external_delegate = GetTtsPlatform()->GetExternalPlatformDelegate();
  if (external_delegate) {
    external_delegate->Stop(source_url);
    return;
  }

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
      current_utterance_->GetSrcUrl().DeprecatedGetOriginAsURL() !=
          source_url.DeprecatedGetOriginAsURL())
    return false;

  StopCurrentUtterance();
  return true;
}

void TtsControllerImpl::StopCurrentUtterance() {
  bool spoken_by_remote_engine =
      IsUtteranceSpokenByRemoteEngine(current_utterance_.get());
  if (engine_delegate_ && current_utterance_ &&
      !current_utterance_->GetEngineId().empty() && !spoken_by_remote_engine) {
    engine_delegate_->Stop(current_utterance_.get());
  } else if (current_utterance_ && !current_utterance_->GetEngineId().empty() &&
             spoken_by_remote_engine && remote_engine_delegate_) {
    remote_engine_delegate_->Stop(current_utterance_.get());
  } else if (TtsPlatformReady()) {
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->StopSpeaking();
  }

  if (current_utterance_) {
    current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                   kInvalidLength, std::string());
  }

  FinishCurrentUtterance();
}

void TtsControllerImpl::Pause() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Pause"));

  auto* external_delegate = GetTtsPlatform()->GetExternalPlatformDelegate();
  if (external_delegate) {
    external_delegate->Pause();
    return;
  }

  if (paused_)
    return;

  paused_ = true;
  bool spoken_by_remote_engine =
      IsUtteranceSpokenByRemoteEngine(current_utterance_.get());
  if (engine_delegate_ && current_utterance_ &&
      !current_utterance_->GetEngineId().empty() && !spoken_by_remote_engine) {
    engine_delegate_->Pause(current_utterance_.get());
  } else if (current_utterance_ && !current_utterance_->GetEngineId().empty() &&
             spoken_by_remote_engine && remote_engine_delegate_) {
    remote_engine_delegate_->Pause(current_utterance_.get());
  } else if (current_utterance_) {
    DCHECK(TtsPlatformReady());
    GetTtsPlatform()->ClearError();
    GetTtsPlatform()->Pause();
  }
}

void TtsControllerImpl::Resume() {
  base::RecordAction(base::UserMetricsAction("TextToSpeech.Resume"));
  auto* external_delegate = GetTtsPlatform()->GetExternalPlatformDelegate();
  if (external_delegate) {
    external_delegate->Resume();
    return;
  }

  if (!paused_)
    return;

  paused_ = false;
  bool spoken_by_remote_engine =
      IsUtteranceSpokenByRemoteEngine(current_utterance_.get());
  if (engine_delegate_ && current_utterance_ &&
      !current_utterance_->GetEngineId().empty() && !spoken_by_remote_engine) {
    engine_delegate_->Resume(current_utterance_.get());
  } else if (current_utterance_ && !current_utterance_->GetEngineId().empty() &&
             spoken_by_remote_engine && remote_engine_delegate_) {
    remote_engine_delegate_->Resume(current_utterance_.get());
  } else if (current_utterance_) {
    DCHECK(TtsPlatformReady());
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
      NOTREACHED_IN_MIGRATION();
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

void TtsControllerImpl::OnTtsUtteranceBecameInvalid(int utterance_id) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This handles the case that the utterance originated from the standalone
  // browser becomes invalid, we need to stop
  RemoveUtteranceAndStopIfNeeded(utterance_id);
#else
  NOTREACHED_IN_MIGRATION();
#endif
}

void TtsControllerImpl::GetVoices(BrowserContext* browser_context,
                                  const GURL& source_url,
                                  std::vector<VoiceData>* out_voices) {
  auto* external_delegate = GetTtsPlatform()->GetExternalPlatformDelegate();
  if (external_delegate) {
    external_delegate->GetVoicesForBrowserContext(browser_context, source_url,
                                                  out_voices);
    return;
  }

  GetVoicesInternal(browser_context, source_url, out_voices);
}

void TtsControllerImpl::GetVoicesInternal(BrowserContext* browser_context,
                                          const GURL& source_url,
                                          std::vector<VoiceData>* out_voices) {
  // Initialize GetTtsPlatform first, so that engine_delegate_ can be set
  // if necessary.
  TtsPlatform* tts_platform = GetTtsPlatform();

  DCHECK(tts_platform);
  // Ensure we have all built-in voices loaded. This is a no-op if already
  // loaded.
  tts_platform->LoadBuiltInTtsEngine(browser_context);
  if (TtsPlatformReady())
    tts_platform->GetVoices(out_voices);

  if (browser_context && engine_delegate_ &&
      engine_delegate_->IsBuiltInTtsEngineInitialized(browser_context)) {
    engine_delegate_->GetVoices(browser_context, source_url, out_voices);
  }

  tts_platform->FinalizeVoiceOrdering(*out_voices);

  // Append lacros voices after ash voices.
  if (remote_engine_delegate_) {
    std::vector<VoiceData> crosapi_voices;
    remote_engine_delegate_->GetVoices(browser_context, &crosapi_voices);
    out_voices->insert(out_voices->end(),
                       std::make_move_iterator(crosapi_voices.begin()),
                       std::make_move_iterator(crosapi_voices.end()));
  }

  if (!allow_remote_voices_) {
    auto it =
        std::remove_if(out_voices->begin(), out_voices->end(),
                       [](const VoiceData& voice) { return voice.remote; });
    out_voices->resize(it - out_voices->begin());
  }
}

bool TtsControllerImpl::IsSpeaking() {
  return current_utterance_ != nullptr ||
         (TtsPlatformReady() && GetTtsPlatform()->IsSpeaking());
}

void TtsControllerImpl::VoicesChanged() {
  if (voices_changed_delegates_.empty() || TtsPlatformLoading())
    return;

  // Existence of platform tts indicates explicit requests to tts. Since
  // |VoicesChanged| can occur implicitly, only send if needed.
  for (auto& delegate : voices_changed_delegates_)
    delegate.OnVoicesChanged();

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!GetTtsPlatform()->PlatformImplSupported()) {
    if (!current_utterance_ && !utterance_list_.empty())
      SpeakNextUtterance();
  }
#else
  if (!current_utterance_ && !utterance_list_.empty())
    SpeakNextUtterance();
#endif
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
    if (engine_delegate_ && !current_utterance_->GetEngineId().empty()) {
      engine_delegate_->Stop(current_utterance_.get());
    } else {
      DCHECK(TtsPlatformReady());
      GetTtsPlatform()->ClearError();
      GetTtsPlatform()->StopSpeaking();
    }

    FinishCurrentUtterance();
    SpeakNextUtterance();
  }
}

void TtsControllerImpl::SetTtsEngineDelegate(TtsEngineDelegate* delegate) {
  engine_delegate_ = delegate;
}

TtsEngineDelegate* TtsControllerImpl::GetTtsEngineDelegate() {
  return engine_delegate_;
}

void TtsControllerImpl::RefreshVoices() {
  GetTtsPlatform()->RefreshVoices();
}

void TtsControllerImpl::Shutdown() {
  if (tts_platform_)
    tts_platform_->Shutdown();
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  DCHECK(tts_platform_);
  return tts_platform_;
}

bool TtsControllerImpl::TtsPlatformReady() {
  TtsPlatform* tts_platform = GetTtsPlatform();
  return tts_platform->PlatformImplSupported() &&
         tts_platform->PlatformImplInitialized();
}

bool TtsControllerImpl::TtsPlatformLoading() {
  // If the platform implementation is supported, it is considered to be in
  // loading state until the platform is inititialized. Typically, that means
  // the libraries are loaded and the voices are being loaded.
  TtsPlatform* tts_platform = GetTtsPlatform();
  return tts_platform->PlatformImplSupported() &&
         !tts_platform->PlatformImplInitialized();
}

void TtsControllerImpl::SpeakNow(std::unique_ptr<TtsUtterance> utterance) {
  // Get all available voices and try to find a matching voice.
  std::vector<VoiceData> voices;
  GetVoices(utterance->GetBrowserContext(), utterance->GetSrcUrl(), &voices);

  // Get the best matching voice. If nothing matches, just set "native"
  // to true because that might trigger deferred loading of native voices.
  // TODO(katie): Move most of the GetMatchingVoice logic into content/ and
  // use the TTS controller delegate to get chrome-specific info as needed.
  int index = GetMatchingVoice(utterance.get(), voices);
  VoiceData voice;
  if (index >= 0) {
    voice = voices[index];
  } else {
    voice.native = true;
    voice.engine_id = utterance->GetEngineId();
    voice.name = utterance->GetVoiceName();
    voice.lang = utterance->GetLang();
  }

  UpdateUtteranceDefaults(utterance.get());

  GetTtsPlatform()->WillSpeakUtteranceWithVoice(utterance.get(), voice);

  base::RecordAction(base::UserMetricsAction("TextToSpeech.Speak"));
  UMA_HISTOGRAM_COUNTS_100000("TextToSpeech.Utterance.Rate",
                              utterance->GetContinuousParameters().rate);
  UMA_HISTOGRAM_COUNTS_100000("TextToSpeech.Utterance.TextLength",
                              utterance->GetText().size());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.FromExtensionAPI",
                        !utterance->GetSrcUrl().is_empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.HasVoiceName",
                        !utterance->GetVoiceName().empty());
  UMA_HISTOGRAM_BOOLEAN("TextToSpeech.Utterance.Native", voice.native);

  if (!voice.native) {
#if !BUILDFLAG(IS_ANDROID)
    DCHECK(!voice.engine_id.empty());
    SetCurrentUtterance(std::move(utterance));
    current_utterance_->SetEngineId(voice.engine_id);
    if (voice.from_remote_tts_engine) {
      DCHECK(remote_engine_delegate_);
      TtsUtteranceImpl* utterance_impl =
          AsUtteranceImpl(current_utterance_.get());
      utterance_impl->set_spoken_by_remote_engine(true);
      remote_engine_delegate_->Speak(current_utterance_.get(), voice);
    } else if (engine_delegate_) {
      engine_delegate_->Speak(current_utterance_.get(), voice);
    }

    bool sends_end_event =
        voice.events.find(TTS_EVENT_END) != voice.events.end();
    if (!sends_end_event) {
      current_utterance_->Finish();
      SetCurrentUtterance(nullptr);
      SpeakNextUtterance();
    }
#endif  // !BUILDFLAG(IS_ANDROID)
  } else {
    // It's possible for certain platforms to send start events immediately
    // during |speak|.
    SetCurrentUtterance(std::move(utterance));
    if (TtsPlatformReady()) {
      GetTtsPlatform()->ClearError();
      GetTtsPlatform()->Speak(
          current_utterance_->GetId(), current_utterance_->GetText(),
          current_utterance_->GetLang(), voice,
          current_utterance_->GetContinuousParameters(),
          base::BindOnce(&TtsControllerImpl::OnSpeakFinished,
                         base::Unretained(this), current_utterance_->GetId()));
    } else {
      // The TTS platform is not supported.
      OnSpeakFinished(current_utterance_->GetId(), false);
    }
  }
}

void TtsControllerImpl::OnSpeakFinished(int utterance_id, bool success) {
  if (success)
    return;

  // Since OnSpeakFinished could run asynchronously, it is possible that the
  // current utterance has changed. Ignore any such spurious callbacks.
  if (!current_utterance_ || current_utterance_->GetId() != utterance_id)
    return;

  // If the native voice wasn't able to process this speech, see if the browser
  // has built-in TTS that crashed and needs re-loading or the utterance came
  // from a profile that no longer exists e.g. login.
  // The controller only ends up here if we had at some point completely
  // initialized native tts and tts engine delegate (see SpeakOrEnqueue), so
  // drop the utterance from re-processing.
  GetTtsPlatform()->LoadBuiltInTtsEngine(
      current_utterance_->GetBrowserContext());

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
  if (!current_utterance_)
    return;

  if (!current_utterance_->IsFinished()) {
    current_utterance_->OnTtsEvent(TTS_EVENT_INTERRUPTED, kInvalidCharIndex,
                                   kInvalidLength, std::string());
  }

  SetCurrentUtterance(nullptr);
}

void TtsControllerImpl::SpeakNextUtterance() {
  if (paused_)
    return;

  // Start speaking the next utterance in the queue.  Keep trying in case
  // one fails but there are still more in the queue to try.
  TtsUtterance* previous_utterance = nullptr;
  while (!utterance_list_.empty() && !current_utterance_) {
    std::unique_ptr<TtsUtterance> utterance =
        std::move(utterance_list_.front());
    utterance_list_.pop_front();
    DCHECK(previous_utterance != utterance.get());

    if (ShouldSpeakUtterance(utterance.get()))
      SpeakNow(std::move(utterance));
    else
      utterance->Finish();

    previous_utterance = utterance.get();
  }
}

void TtsControllerImpl::UpdateUtteranceDefaults(TtsUtterance* utterance) {
  double rate = utterance->GetContinuousParameters().rate;
  double pitch = utterance->GetContinuousParameters().pitch;
  double volume = utterance->GetContinuousParameters().volume;
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
      utterance,
      data_decoder::mojom::XmlParser::WhitespaceBehavior::kPreserveSignificant,
      base::BindOnce(&TtsControllerImpl::StripSSMLHelper, utterance,
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
  if (!result.has_value()) {
    std::move(on_ssml_parsed).Run(utterance);
    return;
  }

  std::string root_tag_name;
  data_decoder::GetXmlElementTagName(*result, &root_tag_name);
  // Root element must be <speak>.
  if (root_tag_name.compare("speak") != 0) {
    std::move(on_ssml_parsed).Run(utterance);
    return;
  }

  std::string parsed_text;
  // Change from unique_ptr to base::Value* so recursion will work.
  PopulateParsedText(&parsed_text, &*result);

  // Run with parsed_text.
  std::move(on_ssml_parsed).Run(parsed_text);
}

void TtsControllerImpl::PopulateParsedText(std::string* parsed_text,
                                           const base::Value* element) {
  DCHECK(parsed_text);
  if (!element || !element->is_dict()) {
    return;
  }
  // Add element's text if present.
  // Note: We don't use data_decoder::GetXmlElementText because it gets the text
  // of element's first child, not text of current element.
  const std::string* text_value =
      element->GetDict().FindString(data_decoder::mojom::XmlParser::kTextKey);
  if (text_value)
    *parsed_text += *text_value;

  const base::Value::List* children =
      data_decoder::GetXmlElementChildren(*element);
  if (!children) {
    return;
  }

  for (const auto& entry : *children) {
    // We need to iterate over all children because some text elements are
    // nested within other types of elements, such as <emphasis> tags.
    PopulateParsedText(parsed_text, &entry);
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  TtsControllerDelegate* delegate = GetTtsControllerDelegate();
  std::unique_ptr<TtsControllerDelegate::PreferredVoiceIds> preferred_ids =
      delegate ? delegate->GetPreferredVoiceIdsForUtterance(utterance)
               : nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
      std::string voice_language =
          base::ToLowerASCII(l10n_util::GetLanguage(voice.lang));
      std::string voice_country =
          base::ToLowerASCII(l10n_util::GetCountry(voice.lang));
      std::string utterance_language =
          base::ToLowerASCII(l10n_util::GetLanguage(utterance->GetLang()));
      std::string utterance_country =
          base::ToLowerASCII(l10n_util::GetCountry(utterance->GetLang()));

      // An exact locale match is worth more than a partial match.
      // Convert locales to lowercase to handle cases like "en-us" vs. "en-US".
      // Cases where language and country match should score the same as an
      // exact match.
      if (voice_language == utterance_language &&
          (voice_country == utterance_country ||
           (utterance_country.empty() && voice_language == voice_country) ||
           (voice_country.empty() &&
            utterance_language == utterance_country))) {
        score += 128;
      } else if (voice_language == utterance_language) {
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    // Finally, prefer system language.
    if (!voice.lang.empty()) {
      if (voice.lang == app_lang) {
        score += 2;
      } else if (base::EqualsCaseInsensitiveASCII(
                     l10n_util::GetLanguage(voice.lang),
                     l10n_util::GetLanguage(app_lang))) {
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
              ? AsUtteranceImpl(current_utterance_.get())->GetWebContents()
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
    if (utterance_impl->GetWebContents() == wc) {
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

void TtsControllerImpl::RemoveUtteranceAndStopIfNeeded(int utterance_id) {
  for (std::list<std::unique_ptr<TtsUtterance>>::iterator it =
           utterance_list_.begin();
       it != utterance_list_.end(); ++it) {
    if ((*it)->GetId() == utterance_id) {
      TtsUtteranceImpl* utterance_impl = AsUtteranceImpl((*it).get());
      utterance_impl->Finish();
      utterance_list_.erase(it);
      break;
    }
  }

  const bool stopped = StopCurrentUtteranceIfMatches(utterance_id);
  if (stopped)
    SpeakNextUtterance();
}

bool TtsControllerImpl::StopCurrentUtteranceIfMatches(int utterance_id) {
  paused_ = false;

  if (current_utterance_->GetId() != utterance_id)
    return false;

  StopCurrentUtterance();
  return true;
}

bool TtsControllerImpl::ShouldSpeakUtterance(TtsUtterance* utterance) {
  TtsUtteranceImpl* utterance_impl = AsUtteranceImpl(utterance);
  if (!utterance_impl->was_created_with_web_contents() ||
      utterance_impl->ShouldAlwaysBeSpoken()) {
    return true;
  }

  // If the WebContents that created the utterance has been destroyed, don't
  // speak it.
  if (!utterance_impl->GetWebContents())
    return false;

  // Allow speaking if either the WebContents is visible, or the WebContents
  // isn't required to be visible before speaking.
  return !stop_speaking_when_hidden_ ||
         utterance_impl->GetWebContents()->GetVisibility() !=
             Visibility::HIDDEN;
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

void TtsControllerImpl::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  switch (type) {
      // Non-cellular connections.
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_ETHERNET:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_BLUETOOTH:
      allow_remote_voices_ = true;
      break;

      // Cellular connections.
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_4G:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE:
    case net::NetworkChangeNotifier::ConnectionType::CONNECTION_5G:
      allow_remote_voices_ = false;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void TtsControllerImpl::SetRemoteTtsEngineDelegate(
    RemoteTtsEngineDelegate* delegate) {
  remote_engine_delegate_ = delegate;
}

}  // namespace content
