// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/speech_synthesis_impl.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/speech/tts_utterance_impl.h"
#include "content/public/browser/web_contents.h"

namespace content {
namespace {

// The lifetime of instances of this class is manually bound to the lifetime of
// the associated TtsUtterance. See OnTtsEvent.
class EventThunk : public UtteranceEventDelegate {
 public:
  explicit EventThunk(
      mojo::PendingRemote<blink::mojom::SpeechSynthesisClient> client)
      : client_(std::move(client)) {}
  ~EventThunk() override = default;

  // UtteranceEventDelegate methods:
  void OnTtsEvent(TtsUtterance* utterance,
                  TtsEventType event_type,
                  int char_index,
                  int char_length,
                  const std::string& error_message) override {
    // These values are unsigned in the web speech API, so -1 cannot be used as
    // a sentinel value. Use 0 instead to match web standards.
    char_index = std::max(char_index, 0);
    char_length = std::max(char_length, 0);

    switch (event_type) {
      case TTS_EVENT_START:
        client_->OnStartedSpeaking();
        break;
      case TTS_EVENT_END:
        client_->OnFinishedSpeaking(
            blink::mojom::SpeechSynthesisErrorCode::kNoError);
        break;
      case TTS_EVENT_INTERRUPTED:
        client_->OnFinishedSpeaking(
            blink::mojom::SpeechSynthesisErrorCode::kInterrupted);
        break;
      case TTS_EVENT_CANCELLED:
        client_->OnFinishedSpeaking(
            blink::mojom::SpeechSynthesisErrorCode::kCancelled);
        break;
      case TTS_EVENT_WORD:
        client_->OnEncounteredWordBoundary(char_index, char_length);
        break;
      case TTS_EVENT_SENTENCE:
        client_->OnEncounteredSentenceBoundary(char_index, 0);
        break;
      case TTS_EVENT_MARKER:
        // The web platform API does not support this event.
        break;
      case TTS_EVENT_ERROR:
        // The web platform API does not support error text.
        client_->OnEncounteredSpeakingError();
        break;
      case TTS_EVENT_PAUSE:
        client_->OnPausedSpeaking();
        break;
      case TTS_EVENT_RESUME:
        client_->OnResumedSpeaking();
        break;
    }

    if (utterance->IsFinished())
      delete this;
  }

 private:
  mojo::Remote<blink::mojom::SpeechSynthesisClient> client_;
};

void SendVoiceListToObserver(
    blink::mojom::SpeechSynthesisVoiceListObserver* observer,
    const std::vector<VoiceData>& voices) {
  std::vector<blink::mojom::SpeechSynthesisVoicePtr> out_voices;
  out_voices.resize(voices.size());
  for (size_t i = 0; i < voices.size(); ++i) {
    blink::mojom::SpeechSynthesisVoicePtr& out_voice = out_voices[i];
    out_voice = blink::mojom::SpeechSynthesisVoice::New();
    out_voice->voice_uri = voices[i].name;
    out_voice->name = voices[i].name;
    out_voice->lang = voices[i].lang;
    out_voice->is_local_service = !voices[i].remote;
    out_voice->is_default = (i == 0);
  }
  observer->OnSetVoiceList(std::move(out_voices));
}

}  // namespace

SpeechSynthesisImpl::SpeechSynthesisImpl(BrowserContext* browser_context,
                                         RenderFrameHostImpl* rfh)
    : browser_context_(browser_context),
      web_contents_(WebContents::FromRenderFrameHost((rfh))) {
  DCHECK(browser_context_);
  DCHECK(web_contents_);
  TtsController::GetInstance()->AddVoicesChangedDelegate(this);
}

SpeechSynthesisImpl::~SpeechSynthesisImpl() {
  TtsController::GetInstance()->RemoveVoicesChangedDelegate(this);

  // NOTE: Some EventThunk instances may outlive this class, and that's okay.
  // They have their lifetime bound to their associated TtsUtterance instance,
  // and the TtsController manages the lifetime of those.
}

void SpeechSynthesisImpl::AddReceiver(
    mojo::PendingReceiver<blink::mojom::SpeechSynthesis> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void SpeechSynthesisImpl::AddVoiceListObserver(
    mojo::PendingRemote<blink::mojom::SpeechSynthesisVoiceListObserver>
        pending_observer) {
  mojo::Remote<blink::mojom::SpeechSynthesisVoiceListObserver> observer(
      std::move(pending_observer));

  std::vector<VoiceData> voices;
  TtsController::GetInstance()->GetVoices(browser_context_, GURL(), &voices);
  SendVoiceListToObserver(observer.get(), voices);

  observer_set_.Add(std::move(observer));
}

void SpeechSynthesisImpl::Speak(
    blink::mojom::SpeechSynthesisUtterancePtr utterance,
    mojo::PendingRemote<blink::mojom::SpeechSynthesisClient> client) {
  if (web_contents_->IsAudioMuted())
    return;

  std::unique_ptr<TtsUtterance> tts_utterance =
      std::make_unique<TtsUtteranceImpl>(browser_context_, web_contents_);
  tts_utterance->SetText(utterance->text);
  tts_utterance->SetLang(utterance->lang);
  tts_utterance->SetVoiceName(utterance->voice);
  tts_utterance->SetShouldClearQueue(false);
  tts_utterance->SetContinuousParameters(utterance->rate, utterance->pitch,
                                         utterance->volume);

  // See comments on EventThunk about how lifetime of this instance is managed.
  tts_utterance->SetEventDelegate(new EventThunk(std::move(client)));

  TtsController::GetInstance()->SpeakOrEnqueue(std::move(tts_utterance));
}

void SpeechSynthesisImpl::Pause() {
  TtsController::GetInstance()->Pause();
}

void SpeechSynthesisImpl::Resume() {
  TtsController::GetInstance()->Resume();
}

void SpeechSynthesisImpl::Cancel() {
  TtsController::GetInstance()->Stop();
}

void SpeechSynthesisImpl::OnVoicesChanged() {
  std::vector<VoiceData> voices;
  TtsController::GetInstance()->GetVoices(browser_context_, GURL(), &voices);
  for (auto& observer : observer_set_)
    SendVoiceListToObserver(observer.get(), voices);
}

}  // namespace content
