// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_SYNTHESIS_IMPL_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_SYNTHESIS_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/tts_controller.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"

namespace content {
class BrowserContext;
class RenderFrameHostImpl;

// Back-end for the web speech synthesis API; dispatches speech requests to
// content::TtsController and forwards voice lists and events back to the
// requesting renderer.
class SpeechSynthesisImpl : public blink::mojom::SpeechSynthesis,
                            public VoicesChangedDelegate {
 public:
  SpeechSynthesisImpl(BrowserContext* browser_context,
                      RenderFrameHostImpl* rfh);
  ~SpeechSynthesisImpl() override;

  SpeechSynthesisImpl(const SpeechSynthesisImpl&) = delete;
  SpeechSynthesisImpl& operator=(const SpeechSynthesisImpl&) = delete;

  void AddReceiver(
      mojo::PendingReceiver<blink::mojom::SpeechSynthesis> receiver);

  // blink::mojom::SpeechSynthesis methods:
  void AddVoiceListObserver(
      mojo::PendingRemote<blink::mojom::SpeechSynthesisVoiceListObserver>
          observer) override;
  void Speak(
      blink::mojom::SpeechSynthesisUtterancePtr utterance,
      mojo::PendingRemote<blink::mojom::SpeechSynthesisClient> client) override;
  void Pause() override;
  void Resume() override;
  void Cancel() override;

  // VoicesChangedDelegate methods:
  void OnVoicesChanged() override;

 private:
  raw_ptr<BrowserContext> browser_context_;
  raw_ptr<WebContents> web_contents_;

  mojo::ReceiverSet<blink::mojom::SpeechSynthesis> receiver_set_;
  mojo::RemoteSet<blink::mojom::SpeechSynthesisVoiceListObserver> observer_set_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_SYNTHESIS_IMPL_H_
