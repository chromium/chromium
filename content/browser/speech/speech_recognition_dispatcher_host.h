// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/speech_recognition_event_listener.h"
#include "media/mojo/mojom/speech_recognizer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace network {
class PendingSharedURLLoaderFactory;
}

namespace url {
class Origin;
}

namespace content {

class SpeechRecognitionManager;

// SpeechRecognitionDispatcherHost is an implementation of the SpeechRecognizer
// interface that allows a RenderFrame to start a speech recognition session
// in the browser process, by communicating with SpeechRecognitionManager.
class SpeechRecognitionDispatcherHost : public media::mojom::SpeechRecognizer {
 public:
  SpeechRecognitionDispatcherHost(int render_process_id, int render_frame_id);

  SpeechRecognitionDispatcherHost(const SpeechRecognitionDispatcherHost&) =
      delete;
  SpeechRecognitionDispatcherHost& operator=(
      const SpeechRecognitionDispatcherHost&) = delete;

  ~SpeechRecognitionDispatcherHost() override;
  static void Create(
      int render_process_id,
      int render_frame_id,
      mojo::PendingReceiver<media::mojom::SpeechRecognizer> receiver);
  base::WeakPtr<SpeechRecognitionDispatcherHost> AsWeakPtr();

  // media::mojom::SpeechRecognizer implementation
  void Start(
      media::mojom::StartSpeechRecognitionRequestParamsPtr params) override;

 private:
  static void StartRequestOnUI(
      base::WeakPtr<SpeechRecognitionDispatcherHost>
          speech_recognition_dispatcher_host,
      int render_process_id,
      int render_frame_id,
      media::mojom::StartSpeechRecognitionRequestParamsPtr params);
  void StartSessionOnIO(
      media::mojom::StartSpeechRecognitionRequestParamsPtr params,
      int embedder_render_process_id,
      int embedder_render_frame_id,
      const url::Origin& origin,
      std::unique_ptr<network::PendingSharedURLLoaderFactory>
          pending_shared_url_loader_factory,
      const std::string& accept_language);

  const int render_process_id_;
  const int render_frame_id_;

  // Used for posting asynchronous tasks (on the IO thread) without worrying
  // about this class being destroyed in the meanwhile (due to browser shutdown)
  // since tasks pending on a destroyed WeakPtr are automatically discarded.
  base::WeakPtrFactory<SpeechRecognitionDispatcherHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_SPEECH_RECOGNITION_DISPATCHER_HOST_H_
