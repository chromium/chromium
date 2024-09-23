// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/speech/tts_platform_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/common/content_switches.h"
#include "library_loaders/libspeechd.h"

namespace content {

namespace {

struct SPDChromeVoice {
  std::string name;
  std::string module;
  std::string language;
};

using PlatformVoices = std::map<std::string, SPDChromeVoice>;

constexpr int kInvalidUtteranceId = -1;
constexpr int kInvalidMessageUid = -1;

}  // namespace

class TtsPlatformImplBackgroundWorker {
 public:
  TtsPlatformImplBackgroundWorker() = default;
  TtsPlatformImplBackgroundWorker(const TtsPlatformImplBackgroundWorker&) =
      delete;
  TtsPlatformImplBackgroundWorker& operator=(
      const TtsPlatformImplBackgroundWorker&) = delete;
  ~TtsPlatformImplBackgroundWorker() = default;

  void Initialize();

  void ProcessSpeech(int utterance_id,
                     const std::string& parsed_utterance,
                     const std::string& lang,
                     float rate,
                     float pitch,
                     SPDChromeVoice voice,
                     base::OnceCallback<void(bool)> on_speak_finished);

  void Pause();
  void Resume();
  void StopSpeaking();
  void Shutdown();

 private:
  bool InitializeSpeechd();
  void InitializeVoices(PlatformVoices*);
  void OpenConnection();
  void CloseConnection();

  void OnSpeechEvent(int msg_id, SPDNotificationType type);

  // Send an TTS event notification to the TTS controller.
  void SendTtsEvent(int utterance_id,
                    TtsEventType event_type,
                    int char_index,
                    int length = -1);

  static void NotificationCallback(size_t msg_id,
                                   size_t client_id,
                                   SPDNotificationType type);

  static void IndexMarkCallback(size_t msg_id,
                                size_t client_id,
                                SPDNotificationType state,
                                char* index_mark);

  LibSpeechdLoader libspeechd_loader_;
  raw_ptr<SPDConnection> conn_ = nullptr;
  int msg_uid_ = kInvalidMessageUid;

  // These apply to the current utterance only that is currently being
  // processed.
  int utterance_id_ = kInvalidUtteranceId;
  size_t utterance_length_ = 0;
  size_t utterance_char_position_ = 0;
};

class TtsPlatformImplLinux : public TtsPlatformImpl {
 public:
  TtsPlatformImplLinux(const TtsPlatformImplLinux&) = delete;
  TtsPlatformImplLinux& operator=(const TtsPlatformImplLinux&) = delete;

  bool PlatformImplSupported() override;
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

  void OnInitialized(bool success, PlatformVoices voices);
  void OnSpeakScheduled(base::OnceCallback<void(bool)> on_speak_finished,
                        bool success);
  void OnSpeakFinished(int utterance_id);

  base::SequenceBound<TtsPlatformImplBackgroundWorker>* worker() {
    return &worker_;
  }

  // Get the single instance of this class.
  static TtsPlatformImplLinux* GetInstance();

 private:
  friend base::NoDestructor<TtsPlatformImplLinux>;
  TtsPlatformImplLinux();

  void ProcessSpeech(int utterance_id,
                     const std::string& lang,
                     const VoiceData& voice,
                     const UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished,
                     const std::string& parsed_utterance);

  // Holds the platform state.
  bool is_supported_ = false;
  bool is_initialized_ = false;
  bool is_speaking_ = false;
  bool paused_ = false;

  // The current utterance being spoke.
  int utterance_id_ = kInvalidUtteranceId;

  // Map a string composed of a voicename and module to the voicename. Used to
  // uniquely identify a voice across all available modules.
  PlatformVoices voices_;

  // Hold the state and the code of the background implementation.
  base::SequenceBound<TtsPlatformImplBackgroundWorker> worker_;
};

//
// TtsPlatformImplBackgroundWorker
//

void TtsPlatformImplBackgroundWorker::Initialize() {
  PlatformVoices voices;
  if (InitializeSpeechd()) {
    OpenConnection();
    InitializeVoices(&voices);
  }

  bool success = (conn_ != nullptr);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TtsPlatformImplLinux::OnInitialized,
                     base::Unretained(TtsPlatformImplLinux::GetInstance()),
                     success, std::move(voices)));
}

void TtsPlatformImplBackgroundWorker::ProcessSpeech(
    int utterance_id,
    const std::string& parsed_utterance,
    const std::string& lang,
    float rate,
    float pitch,
    SPDChromeVoice voice,
    base::OnceCallback<void(bool)> on_speak_finished) {
  if (!conn_) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_speak_finished), false));
    return;
  }

  libspeechd_loader_.spd_set_output_module(conn_, voice.module.c_str());
  libspeechd_loader_.spd_set_synthesis_voice(conn_, voice.name.c_str());

  // Map our multiplicative range to Speech Dispatcher's linear range.
  // .334 = -100.
  // 3 = 100.
  libspeechd_loader_.spd_set_voice_rate(conn_, 100 * log10(rate) / log10(3));
  libspeechd_loader_.spd_set_voice_pitch(conn_, 100 * log10(pitch) / log10(3));

  // Support languages other than the default
  if (!lang.empty())
    libspeechd_loader_.spd_set_language(conn_, lang.c_str());

  utterance_id_ = utterance_id;
  utterance_char_position_ = 0;
  utterance_length_ = parsed_utterance.size();

  // spd_say(...) returns msg_uid on success, -1 otherwise. Each call to spd_say
  // returns a different msg_uid.
  msg_uid_ =
      libspeechd_loader_.spd_say(conn_, SPD_TEXT, parsed_utterance.c_str());

  bool success = (msg_uid_ != kInvalidMessageUid);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(on_speak_finished), success));
}

void TtsPlatformImplBackgroundWorker::Pause() {
  if (conn_ && msg_uid_ != kInvalidMessageUid)
    libspeechd_loader_.spd_pause(conn_);
}

void TtsPlatformImplBackgroundWorker::Resume() {
  if (conn_ && msg_uid_ != kInvalidMessageUid)
    libspeechd_loader_.spd_resume(conn_);
}

void TtsPlatformImplBackgroundWorker::StopSpeaking() {
  if (conn_ && msg_uid_ != kInvalidMessageUid) {
    int result = libspeechd_loader_.spd_stop(conn_);
    if (result == -1) {
      CloseConnection();
      OpenConnection();
    }
    msg_uid_ = kInvalidMessageUid;
    utterance_id_ = kInvalidUtteranceId;
  }
}

void TtsPlatformImplBackgroundWorker::Shutdown() {
  CloseConnection();
}

bool TtsPlatformImplBackgroundWorker::InitializeSpeechd() {
  return libspeechd_loader_.Load("libspeechd.so.2");
}

void TtsPlatformImplBackgroundWorker::InitializeVoices(PlatformVoices* voices) {
  if (!conn_)
    return;

  char** modules = libspeechd_loader_.spd_list_modules(conn_);
  if (!modules)
    return;
  for (int i = 0; modules[i]; i++) {
    char* module = modules[i];
    libspeechd_loader_.spd_set_output_module(conn_, module);
    SPDVoice** spd_voices = libspeechd_loader_.spd_list_synthesis_voices(conn_);
    if (!spd_voices) {
      free(module);
      continue;
    }
    for (int j = 0; spd_voices[j]; j++) {
      SPDVoice* spd_voice = spd_voices[j];
      SPDChromeVoice spd_data;
      spd_data.name = spd_voice->name;
      spd_data.module = module;
      spd_data.language = spd_voice->language;
      std::string key;
      key.append(spd_data.name);
      key.append(" ");
      key.append(spd_data.module);
      voices->insert(std::pair<std::string, SPDChromeVoice>(key, spd_data));
      free(spd_voices[j]);
    }
    free(modules[i]);
  }
}

void TtsPlatformImplBackgroundWorker::OpenConnection() {
  {
    // spd_open has memory leaks which are hard to suppress.
    // http://crbug.com/317360
    ANNOTATE_SCOPED_MEMORY_LEAK;
    conn_ = libspeechd_loader_.spd_open("chrome", "extension_api", nullptr,
                                        SPD_MODE_THREADED);
  }
  if (!conn_)
    return;

  // Register callbacks for all events.
  conn_->callback_begin = conn_->callback_end = conn_->callback_cancel =
      conn_->callback_pause = conn_->callback_resume =
          &TtsPlatformImplBackgroundWorker::NotificationCallback;

  conn_->callback_im = &TtsPlatformImplBackgroundWorker::IndexMarkCallback;

  libspeechd_loader_.spd_set_notification_on(conn_, SPD_BEGIN);
  libspeechd_loader_.spd_set_notification_on(conn_, SPD_END);
  libspeechd_loader_.spd_set_notification_on(conn_, SPD_CANCEL);
  libspeechd_loader_.spd_set_notification_on(conn_, SPD_PAUSE);
  libspeechd_loader_.spd_set_notification_on(conn_, SPD_RESUME);
}

void TtsPlatformImplBackgroundWorker::CloseConnection() {
  if (conn_) {
    libspeechd_loader_.spd_close(conn_);
    conn_ = nullptr;
  }
}

void TtsPlatformImplBackgroundWorker::OnSpeechEvent(int msg_id,
                                                    SPDNotificationType type) {
  if (!conn_ || msg_id != msg_uid_)
    return;

  switch (type) {
    case SPD_EVENT_BEGIN:
      utterance_char_position_ = 0;
      SendTtsEvent(utterance_id_, TTS_EVENT_START, utterance_char_position_,
                   -1);
      break;
    case SPD_EVENT_RESUME:
      SendTtsEvent(utterance_id_, TTS_EVENT_RESUME, utterance_char_position_,
                   -1);
      break;
    case SPD_EVENT_END:
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&TtsPlatformImplLinux::OnSpeakFinished,
                         base::Unretained(TtsPlatformImplLinux::GetInstance()),
                         utterance_id_));

      utterance_char_position_ = utterance_length_;
      SendTtsEvent(utterance_id_, TTS_EVENT_END, utterance_char_position_, 0);
      break;
    case SPD_EVENT_PAUSE:
      SendTtsEvent(utterance_id_, TTS_EVENT_PAUSE, utterance_char_position_,
                   -1);
      break;
    case SPD_EVENT_CANCEL:
      SendTtsEvent(utterance_id_, TTS_EVENT_CANCELLED, utterance_char_position_,
                   -1);
      break;
    case SPD_EVENT_INDEX_MARK:
      // TODO: Can we get length from linux? If so, update
      // utterance_char_position_.
      SendTtsEvent(utterance_id_, TTS_EVENT_MARKER, utterance_char_position_,
                   -1);
      break;
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

// static
void TtsPlatformImplBackgroundWorker::NotificationCallback(
    size_t msg_id,
    size_t client_id,
    SPDNotificationType type) {
  TtsPlatformImplLinux::GetInstance()
      ->worker()
      ->AsyncCall(&TtsPlatformImplBackgroundWorker::OnSpeechEvent)
      .WithArgs(msg_id, type);
}

// static
void TtsPlatformImplBackgroundWorker::IndexMarkCallback(
    size_t msg_id,
    size_t client_id,
    SPDNotificationType type,
    char* index_mark) {
  // TODO(dtseng): index_mark appears to specify an index type supplied by a
  // client. Need to explore how this is used before hooking it up with existing
  // word, sentence events.
  TtsPlatformImplLinux::GetInstance()
      ->worker()
      ->AsyncCall(&TtsPlatformImplBackgroundWorker::OnSpeechEvent)
      .WithArgs(msg_id, type);
}

//
// TtsPlatformImplLinux
//

TtsPlatformImplLinux::TtsPlatformImplLinux()
    : worker_(base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kEnableSpeechDispatcher))
    return;

  // The TTS platform is supported. The Tts platform initialisation will happen
  // on a worker thread and it will become initialized.
  is_supported_ = true;
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Initialize);
}

bool TtsPlatformImplLinux::PlatformImplSupported() {
  return is_supported_;
}

bool TtsPlatformImplLinux::PlatformImplInitialized() {
  return is_initialized_;
}

void TtsPlatformImplLinux::Speak(
    int utterance_id,
    const std::string& utterance,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(PlatformImplInitialized());

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
      base::BindOnce(&TtsPlatformImplLinux::ProcessSpeech,
                     base::Unretained(this), utterance_id, lang, voice, params,
                     base::BindOnce(&TtsPlatformImplLinux::OnSpeakScheduled,
                                    base::Unretained(this),
                                    std::move(on_speak_finished))));
}

bool TtsPlatformImplLinux::StopSpeaking() {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(PlatformImplInitialized());

  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::StopSpeaking);
  paused_ = false;

  is_speaking_ = false;
  utterance_id_ = kInvalidUtteranceId;

  return true;
}

void TtsPlatformImplLinux::Pause() {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(PlatformImplInitialized());

  if (paused_ || !is_speaking_)
    return;

  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Pause);
  paused_ = true;
}

void TtsPlatformImplLinux::Resume() {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(PlatformImplInitialized());

  if (!paused_ || !is_speaking_)
    return;

  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Resume);
  paused_ = false;
}

bool TtsPlatformImplLinux::IsSpeaking() {
  return is_speaking_;
}

void TtsPlatformImplLinux::GetVoices(std::vector<VoiceData>* out_voices) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(PlatformImplInitialized());

  for (auto it = voices_.begin(); it != voices_.end(); ++it) {
    out_voices->push_back(VoiceData());
    VoiceData& voice = out_voices->back();
    voice.native = true;
    voice.name = it->first;
    voice.lang = it->second.language;
    voice.events.insert(TTS_EVENT_START);
    voice.events.insert(TTS_EVENT_END);
    voice.events.insert(TTS_EVENT_CANCELLED);
    voice.events.insert(TTS_EVENT_MARKER);
    voice.events.insert(TTS_EVENT_PAUSE);
    voice.events.insert(TTS_EVENT_RESUME);
  }
}

void TtsPlatformImplLinux::Shutdown() {
  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::Shutdown);
}

void TtsPlatformImplLinux::OnInitialized(bool success, PlatformVoices voices) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (success)
    voices_ = std::move(voices);
  is_initialized_ = true;
  TtsController::GetInstance()->VoicesChanged();
}

void TtsPlatformImplLinux::OnSpeakScheduled(
    base::OnceCallback<void(bool)> on_speak_finished,
    bool success) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(is_speaking_);

  // If the utterance was not able to be emitted, stop the speaking. There
  // won't be any asynchronous TTS event to confirm the end of the speech.
  if (!success) {
    is_speaking_ = false;
    utterance_id_ = kInvalidUtteranceId;
  }

  // Pass the results to our caller.
  std::move(on_speak_finished).Run(success);
}

void TtsPlatformImplLinux::OnSpeakFinished(int utterance_id) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (utterance_id != utterance_id_)
    return;

  DCHECK(is_speaking_);
  DCHECK_NE(utterance_id_, kInvalidUtteranceId);
  is_speaking_ = false;
  utterance_id_ = kInvalidUtteranceId;
}

void TtsPlatformImplLinux::ProcessSpeech(
    int utterance_id,
    const std::string& lang,
    const VoiceData& voice,
    const UtteranceContinuousParameters& params,
    base::OnceCallback<void(bool)> on_speak_finished,
    const std::string& parsed_utterance) {
  DCHECK(BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Speech dispatcher's speech params are around 3x at either limit.
  float rate = std::clamp(static_cast<float>(params.rate), 0.334f, 3.0f);
  float pitch = std::clamp(static_cast<float>(params.pitch), 0.334f, 3.0f);

  SPDChromeVoice matched_voice;
  auto it = voices_.find(voice.name);
  if (it != voices_.end())
    matched_voice = it->second;

  worker_.AsyncCall(&TtsPlatformImplBackgroundWorker::ProcessSpeech)
      .WithArgs(utterance_id, parsed_utterance, lang, rate, pitch,
                matched_voice, std::move(on_speak_finished));
}

// static
TtsPlatformImplLinux* TtsPlatformImplLinux::GetInstance() {
  static base::NoDestructor<TtsPlatformImplLinux> tts_platform;
  return tts_platform.get();
}

// static
TtsPlatformImpl* TtsPlatformImpl::GetInstance() {
  return TtsPlatformImplLinux::GetInstance();
}

}  // namespace content
