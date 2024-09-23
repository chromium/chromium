// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <objbase.h>

#include <math.h>
#include <sapi.h>
#include <stdint.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <algorithm>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/values.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/sphelper.h"
#include "content/browser/speech/tts_platform_impl.h"
#include "content/browser/speech/tts_win_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_controller.h"

namespace content {

namespace {

class TtsPlatformImplWin;
class TtsPlatformImplBackgroundWorker;

constexpr int kInvalidUtteranceId = -1;

// ISpObjectToken key and value names.
const wchar_t kAttributesKey[] = L"Attributes";
const wchar_t kLanguageValue[] = L"Language";

// Original blog detailing how to use this registry.
// https://social.msdn.microsoft.com/Forums/en-US/8bbe761c-69c7-401c-8261-1442935c57c8/why-isnt-my-program-detecting-all-tts-voices
// Microsoft docs on how to view system registry keys.
// https://docs.microsoft.com/en-us/troubleshoot/windows-client/deployment/view-system-registry-with-64-bit-windows
const wchar_t* kSPCategoryOneCoreVoices =
    L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices";

// This COM interface is receiving the TTS events on the ISpVoice asynchronous
// worker thread and is emitting a notification task
// TtsPlatformImplBackgroundWorker::SendTtsEvent(...) on the worker sequence.
class TtsEventSink
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ISpNotifySink> {
 public:
  TtsEventSink(TtsPlatformImplBackgroundWorker* worker,
               scoped_refptr<base::TaskRunner> worker_task_runner)
      : worker_(worker), worker_task_runner_(std::move(worker_task_runner)) {}

  // ISpNotifySink:
  IFACEMETHODIMP Notify(void) override;

  int GetUtteranceId() {
    base::AutoLock lock(lock_);
    return utterance_id_;
  }

  void SetUtteranceId(int utterance_id) {
    base::AutoLock lock(lock_);
    utterance_id_ = utterance_id;
  }

 private:
  // |worker_| is leaky and must never deleted because TtsEventSink posts
  // asynchronous tasks to it.
  raw_ptr<TtsPlatformImplBackgroundWorker> worker_;
  scoped_refptr<base::TaskRunner> worker_task_runner_;

  base::Lock lock_;
  int utterance_id_ GUARDED_BY(lock_);
};

class TtsPlatformImplBackgroundWorker {
 public:
  explicit TtsPlatformImplBackgroundWorker(
      scoped_refptr<base::TaskRunner> task_runner)
      : tts_event_sink_(
            Microsoft::WRL::Make<TtsEventSink>(this, std::move(task_runner))) {}
  TtsPlatformImplBackgroundWorker(const TtsPlatformImplBackgroundWorker&) =
      delete;
  TtsPlatformImplBackgroundWorker& operator=(
      const TtsPlatformImplBackgroundWorker&) = delete;
  ~TtsPlatformImplBackgroundWorker() = default;

  void Initialize();

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const VoiceData& voice,
                     const UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  void StopSpeaking(bool paused);
  void Pause();
  void Resume();
  void Shutdown();

  // This function is called after being notified by the speech synthetizer that
  // there are TTS notifications are available and should be they should be
  // processed.
  void OnSpeechEvent(int utterance_id);

  // Send an TTS event notification to the TTS controller.
  void SendTtsEvent(int utterance_id,
                    TtsEventType event_type,
                    int char_index,
                    int length = -1);

 private:
  void GetVoices(std::vector<VoiceData>* voices);

  // Search the newer OneCore or the older SAPI locations for voice tokens.
  // This ensures new voices are shown and that the method works on Windows 7.
  bool GetVoiceTokens(Microsoft::WRL::ComPtr<IEnumSpObjectTokens>* out_tokens);

  void SetVoiceFromName(const std::string& name);

  // These apply to the current utterance only that is currently being processed
  // on the worker thread. TTS events are dispatched by TtsEventSink to this
  // class and update the current speaking state of the utterance.
  std::string last_voice_name_;
  ULONG stream_number_ = 0u;
  int utterance_id_ = kInvalidUtteranceId;
  size_t utterance_char_position_ = 0u;
  size_t utterance_prefix_length_ = 0u;
  size_t utterance_length_ = 0u;

  // The COM class ISpVoice lives within the COM MTA apartment (worker pool).
  // This interface can not be called on the UI thread since UI thread is
  // COM STA.
  Microsoft::WRL::ComPtr<ISpVoice> speech_synthesizer_;
  Microsoft::WRL::ComPtr<TtsEventSink> tts_event_sink_;
};

class TtsPlatformImplWin : public TtsPlatformImpl {
 public:
  TtsPlatformImplWin(const TtsPlatformImplWin&) = delete;
  TtsPlatformImplWin& operator=(const TtsPlatformImplWin&) = delete;

  bool PlatformImplSupported() override { return true; }
  bool PlatformImplInitialized() override;

  void Speak(int utterance_id,
             const std::string& utterance,
             const std::string& lang,
             const VoiceData& voice,
             const UtteranceContinuousParameters& params,
             base::OnceCallback<void(bool)> on_speak_finished) override;

  bool StopSpeaking() override;

  void Pause() override;

  void Resume() override;

  bool IsSpeaking() override;

  void GetVoices(std::vector<VoiceData>* out_voices) override;

  void Shutdown() override;

  void OnInitializeComplete(bool success, std::vector<VoiceData> voices);
  void OnSpeakScheduled(base::OnceCallback<void(bool)> on_speak_finished,
                        bool success);
  void OnSpeakFinished(int utterance_id);

  // Get the single instance of this class.
  static TtsPlatformImplWin* GetInstance();

 private:
  friend base::NoDestructor<TtsPlatformImplWin>;
  TtsPlatformImplWin();

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const VoiceData& voice,
                     const UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  void FinishCurrentUtterance();

  // These variables hold the platform state.
  bool paused_ = false;
  bool is_speaking_ = false;
  int utterance_id_ = kInvalidUtteranceId;
  bool platform_initialized_ = false;
  std::vector<VoiceData> voices_;

  // Hold the state and the code of the background implementation.
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
  base::SequenceBound<TtsPlatformImplBackgroundWorker> worker_;
};

HRESULT TtsEventSink::Notify() {
  worker_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TtsPlatformImplBackgroundWorker::OnSpeechEvent,
                                base::Unretained(worker_), GetUtteranceId()));
  return S_OK;
}

//
// TtsPlatformImplBackgroundWorker
//

void TtsPlatformImplBackgroundWorker::Initialize() {
  bool success = false;
  std::vector<VoiceData> voices;

  ::CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL,
                     IID_PPV_ARGS(&speech_synthesizer_));
  if (speech_synthesizer_.Get()) {
    ULONGLONG event_mask =
        SPFEI(SPEI_START_INPUT_STREAM) | SPFEI(SPEI_TTS_BOOKMARK) |
        SPFEI(SPEI_WORD_BOUNDARY) | SPFEI(SPEI_SENTENCE_BOUNDARY) |
        SPFEI(SPEI_END_INPUT_STREAM);
    speech_synthesizer_->SetInterest(event_mask, event_mask);
    speech_synthesizer_->SetNotifySink(tts_event_sink_.Get());

    GetVoices(&voices);

    success = true;
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TtsPlatformImplWin::OnInitializeComplete,
                     base::Unretained(TtsPlatformImplWin::GetInstance()),
                     success, std::move(voices)));
}

void TtsPlatformImplBackgroundWorker::ProcessSpeech(
    int utterance_id,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished,
    const std::string& parsed_utterance) {
  DCHECK(speech_synthesizer_.Get());

  SetVoiceFromName(voice.name);

  if (params.rate >= 0.0) {
    // Map our multiplicative range of 0.1x to 10.0x onto Microsoft's
    // linear range of -10 to 10:
    //   0.1 -> -10
    //   1.0 -> 0
    //  10.0 -> 10
    speech_synthesizer_->SetRate(static_cast<int32_t>(10 * log10(params.rate)));
  }

  std::wstring prefix;
  std::wstring suffix;
  if (params.pitch >= 0.0) {
    // The TTS api allows a range of -10 to 10 for speech pitch:
    // https://docs.microsoft.com/en-us/previous-versions/windows/desktop/ms720500(v%3Dvs.85)
    // Note that the API requires an integer value, so be sure to cast the pitch
    // value to an int before calling NumberToWString. TODO(dtseng): cleanup if
    // we ever use any other properties that require xml.
    double adjusted_pitch = std::clamp<double>(params.pitch * 10 - 10, -10, 10);
    std::wstring adjusted_pitch_string =
        base::NumberToWString(static_cast<int>(adjusted_pitch));
    prefix = L"<pitch absmiddle=\"" + adjusted_pitch_string + L"\">";
    suffix = L"</pitch>";
  }

  if (params.volume >= 0.0) {
    // The TTS api allows a range of 0 to 100 for speech volume.
    speech_synthesizer_->SetVolume(static_cast<uint16_t>(params.volume * 100));
  }

  // TODO(dmazzoni): convert SSML to SAPI xml. http://crbug.com/88072

  std::wstring utterance = base::UTF8ToWide(parsed_utterance);
  RemoveXml(utterance);
  std::wstring merged_utterance = prefix + utterance + suffix;

  utterance_id_ = utterance_id;
  utterance_char_position_ = 0;
  utterance_length_ = utterance.size();
  utterance_prefix_length_ = prefix.size();

  tts_event_sink_->SetUtteranceId(utterance_id);

  HRESULT result = speech_synthesizer_->Speak(merged_utterance.c_str(),
                                              SPF_ASYNC, &stream_number_);
  bool success = (result == S_OK);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_speak_finished), success));
}

void TtsPlatformImplWin::FinishCurrentUtterance() {
  if (paused_)
    Resume();

  DCHECK(is_speaking_ || (utterance_id_ == kInvalidUtteranceId));
  is_speaking_ = false;
  utterance_id_ = kInvalidUtteranceId;
}

void TtsPlatformImplBackgroundWorker::StopSpeaking(bool paused) {
  if (speech_synthesizer_.Get()) {
    // Block notifications from the current utterance.
    tts_event_sink_->SetUtteranceId(kInvalidUtteranceId);
    utterance_id_ = kInvalidUtteranceId;

    // Stop speech by speaking nullptr with the purge flag.
    speech_synthesizer_->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);

    // Ensures the synthesizer is not paused after a stop.
    if (paused)
      speech_synthesizer_->Resume();
  }
}

void TtsPlatformImplBackgroundWorker::Pause() {
  if (speech_synthesizer_.Get()) {
    speech_synthesizer_->Pause();
    SendTtsEvent(utterance_id_, TTS_EVENT_PAUSE, utterance_char_position_);
  }
}

void TtsPlatformImplBackgroundWorker::Resume() {
  if (speech_synthesizer_.Get()) {
    speech_synthesizer_->Resume();
    SendTtsEvent(utterance_id_, TTS_EVENT_RESUME, utterance_char_position_);
  }
}

void TtsPlatformImplBackgroundWorker::Shutdown() {
  if (speech_synthesizer_)
    speech_synthesizer_->SetNotifySink(nullptr);
  if (tts_event_sink_) {
    tts_event_sink_->SetUtteranceId(kInvalidUtteranceId);
    utterance_id_ = kInvalidUtteranceId;
  }

  tts_event_sink_ = nullptr;
  speech_synthesizer_ = nullptr;
}

void TtsPlatformImplBackgroundWorker::OnSpeechEvent(int utterance_id) {
  if (!speech_synthesizer_.Get())
    return;

  SPEVENT event;
  while (S_OK == speech_synthesizer_->GetEvents(1, &event, nullptr)) {
    // Ignore notifications that are not related to the current utterance.
    if (event.ulStreamNum != stream_number_ ||
        utterance_id_ == kInvalidUtteranceId || utterance_id != utterance_id_) {
      continue;
    }

    switch (event.eEventId) {
      case SPEI_START_INPUT_STREAM:
        utterance_char_position_ = 0;
        SendTtsEvent(utterance_id_, TTS_EVENT_START, utterance_char_position_);
        break;
      case SPEI_END_INPUT_STREAM:
        GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE,
            base::BindOnce(&TtsPlatformImplWin::OnSpeakFinished,
                           base::Unretained(TtsPlatformImplWin::GetInstance()),
                           utterance_id_));

        utterance_char_position_ = utterance_length_;
        SendTtsEvent(utterance_id_, TTS_EVENT_END, utterance_char_position_);
        break;
      case SPEI_TTS_BOOKMARK:
        SendTtsEvent(utterance_id_, TTS_EVENT_MARKER, utterance_char_position_);
        break;
      case SPEI_WORD_BOUNDARY:
        utterance_char_position_ =
            static_cast<size_t>(event.lParam) - utterance_prefix_length_;
        SendTtsEvent(utterance_id_, TTS_EVENT_WORD, utterance_char_position_,
                     static_cast<ULONG>(event.wParam));

        break;
      case SPEI_SENTENCE_BOUNDARY:
        utterance_char_position_ =
            static_cast<size_t>(event.lParam) - utterance_prefix_length_;
        SendTtsEvent(utterance_id_, TTS_EVENT_SENTENCE,
                     utterance_char_position_);
        break;
      default:
        break;
    }
  }
}

void TtsPlatformImplBackgroundWorker::SendTtsEvent(int utterance_id,
                                                   TtsEventType event_type,
                                                   int char_index,
                                                   int length) {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&TtsController::OnTtsEvent,
                                base::Unretained(TtsController::GetInstance()),
                                utterance_id, event_type, char_index, length,
                                std::string()));
}

void TtsPlatformImplBackgroundWorker::GetVoices(
    std::vector<VoiceData>* out_voices) {
  if (!speech_synthesizer_.Get())
    return;

  Microsoft::WRL::ComPtr<IEnumSpObjectTokens> voice_tokens;
  unsigned long voice_count;
  if (!this->GetVoiceTokens(&voice_tokens)) {
    return;
  }
  if (S_OK != voice_tokens->GetCount(&voice_count))
    return;

  for (unsigned i = 0; i < voice_count; i++) {
    VoiceData voice;

    Microsoft::WRL::ComPtr<ISpObjectToken> voice_token;
    if (S_OK != voice_tokens->Next(1, &voice_token, NULL))
      return;

    base::win::ScopedCoMem<WCHAR> description;
    if (S_OK != SpGetDescription(voice_token.Get(), &description))
      continue;
    voice.name = base::WideToUTF8(description.get());

    Microsoft::WRL::ComPtr<ISpDataKey> attributes;
    if (S_OK != voice_token->OpenKey(kAttributesKey, &attributes))
      continue;

    base::win::ScopedCoMem<WCHAR> language;
    if (S_OK == attributes->GetStringValue(kLanguageValue, &language)) {
      int lcid_value;
      base::HexStringToInt(base::WideToUTF8(language.get()), &lcid_value);
      LCID lcid = MAKELCID(lcid_value, SORT_DEFAULT);
      WCHAR locale_name[LOCALE_NAME_MAX_LENGTH] = {0};
      LCIDToLocaleName(lcid, locale_name, LOCALE_NAME_MAX_LENGTH, 0);
      voice.lang = base::WideToUTF8(locale_name);
    }

    voice.native = true;
    voice.events.insert(TTS_EVENT_START);
    voice.events.insert(TTS_EVENT_END);
    voice.events.insert(TTS_EVENT_MARKER);
    voice.events.insert(TTS_EVENT_WORD);
    voice.events.insert(TTS_EVENT_SENTENCE);
    voice.events.insert(TTS_EVENT_PAUSE);
    voice.events.insert(TTS_EVENT_RESUME);
    out_voices->push_back(voice);
  }
}

void TtsPlatformImplBackgroundWorker::SetVoiceFromName(
    const std::string& name) {
  if (name.empty() || name == last_voice_name_)
    return;

  last_voice_name_ = name;

  Microsoft::WRL::ComPtr<IEnumSpObjectTokens> voice_tokens;
  unsigned long voice_count;
  if (!this->GetVoiceTokens(&voice_tokens)) {
    return;
  }
  if (S_OK != voice_tokens->GetCount(&voice_count))
    return;

  for (unsigned i = 0; i < voice_count; i++) {
    Microsoft::WRL::ComPtr<ISpObjectToken> voice_token;
    if (S_OK != voice_tokens->Next(1, &voice_token, NULL))
      return;

    base::win::ScopedCoMem<WCHAR> description;
    if (S_OK != SpGetDescription(voice_token.Get(), &description))
      continue;
    if (name == base::WideToUTF8(description.get())) {
      speech_synthesizer_->SetVoice(voice_token.Get());
      break;
    }
  }
}

bool TtsPlatformImplBackgroundWorker::GetVoiceTokens(
    Microsoft::WRL::ComPtr<IEnumSpObjectTokens>* out_tokens) {
  if (S_OK ==
      SpEnumTokens(kSPCategoryOneCoreVoices, NULL, NULL, &(*out_tokens))) {
  } else if (S_OK != SpEnumTokens(SPCAT_VOICES, NULL, NULL, &(*out_tokens))) {
    return false;
  }
  return true;
}

//
// TtsPlatformImplWin
//

bool TtsPlatformImplWin::PlatformImplInitialized() {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return platform_initialized_;
}

void TtsPlatformImplWin::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(platform_initialized_);

  // Do not emit utterance if the platform is not ready.
  if (paused_ || is_speaking_) {
    std::move(on_speak_finished).Run(false);
    return;
  }

  // Flag that a utterance is getting emitted. The |is_speaking_| flag will be
  // set back to false when the utterance will be fully spoken, stopped or if
  // the voice synthetizer was not able to emit it.
  is_speaking_ = true;
  utterance_id_ = utterance_id;

  // Parse SSML and process speech.
  TtsController::GetInstance()->StripSSML(
      utterance,
      base::BindOnce(&TtsPlatformImplWin::ProcessSpeech, base::Unretained(this),
                     utterance_id, lang, voice, params,
                     base::BindOnce(&TtsPlatformImplWin::OnSpeakScheduled,
                                    base::Unretained(this),
                                    std::move(on_speak_finished))));
}

bool TtsPlatformImplWin::StopSpeaking() {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::StopSpeaking)
      .WithArgs(paused_);
  paused_ = false;

  is_speaking_ = false;
  utterance_id_ = kInvalidUtteranceId;

  return true;
}

void TtsPlatformImplWin::Pause() {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(platform_initialized_);

  if (paused_ || !is_speaking_)
    return;
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Pause);
  paused_ = true;
}

void TtsPlatformImplWin::Resume() {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(platform_initialized_);

  if (!paused_)
    return;

  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Resume);
  paused_ = false;
}

bool TtsPlatformImplWin::IsSpeaking() {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(platform_initialized_);
  return is_speaking_ && !paused_;
}

void TtsPlatformImplWin::GetVoices(std::vector<VoiceData>* out_voices) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(platform_initialized_);
  out_voices->insert(out_voices->end(), voices_.begin(), voices_.end());
}

void TtsPlatformImplWin::Shutdown() {
  // This is required to ensures the object is released before the COM is
  // uninitialized. Otherwise, this is causing shutdown hangs.
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Shutdown);
}

void TtsPlatformImplWin::OnInitializeComplete(bool success,
                                              std::vector<VoiceData> voices) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (success)
    voices_ = std::move(voices);

  platform_initialized_ = true;
  TtsController::GetInstance()->VoicesChanged();
}

void TtsPlatformImplWin::OnSpeakScheduled(
    base::OnceCallback<void(bool)> on_speak_finished,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(is_speaking_ || (utterance_id_ == kInvalidUtteranceId));
  // If speech was stopped while we were processing the utterance (For example,
  // in the case of a page navigation), then there is nothing left to do. Do not
  // emit an asynchronous TTS event to confirm the end of speech.
  if (!is_speaking_) {
    return;
  }

  // If the utterance was not able to be emitted, stop the speaking. There
  // won't be any asynchronous TTS event to confirm the end of the speech.
  if (!success)
    FinishCurrentUtterance();

  // Pass the results to our caller.
  std::move(on_speak_finished).Run(success);
}

void TtsPlatformImplWin::OnSpeakFinished(int utterance_id) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (utterance_id != utterance_id_)
    return;

  FinishCurrentUtterance();
}

void TtsPlatformImplWin::ProcessSpeech(
    int utterance_id,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished,
    const std::string& parsed_utterance) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::ProcessSpeech)
      .WithArgs(utterance_id, lang, voice, params, std::move(on_speak_finished),
                parsed_utterance);
}

TtsPlatformImplWin::TtsPlatformImplWin()
    : worker_task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      worker_(worker_task_runner_, worker_task_runner_) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Initialize);
}

// static
TtsPlatformImplWin* TtsPlatformImplWin::GetInstance() {
  static base::NoDestructor<TtsPlatformImplWin> tts_platform;
  return tts_platform.get();
}

}  // namespace

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplWin::GetInstance();
}

}  // namespace content
