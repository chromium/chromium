// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/in_process_audio_loopback_stream_creator.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/system_connector.h"
#include "media/audio/audio_device_description.h"
#include "media/base/user_input_monitor.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

// A mojom::RendererAudioInputStreamFactoryClient that holds a
// AudioLoopbackStreamCreator::StreamCreatedCallback. The callback runs when the
// requested audio stream is created.
class StreamCreatedCallbackAdapter final
    : public mojom::RendererAudioInputStreamFactoryClient {
 public:
  explicit StreamCreatedCallbackAdapter(
      const AudioLoopbackStreamCreator::StreamCreatedCallback& callback)
      : callback_(callback) {
    DCHECK(callback_);
  }

  ~StreamCreatedCallbackAdapter() override {}

  // mojom::RendererAudioInputStreamFactoryClient implementation.
  void StreamCreated(
      mojo::PendingRemote<media::mojom::AudioInputStream> stream,
      mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
          client_receiver,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
      bool initially_muted,
      const base::Optional<base::UnguessableToken>& stream_id) override {
    DCHECK(!initially_muted);  // Loopback streams shouldn't be started muted.
    callback_.Run(std::move(stream), std::move(client_receiver),
                  std::move(data_pipe));
  }

 private:
  const AudioLoopbackStreamCreator::StreamCreatedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(StreamCreatedCallbackAdapter);
};

void CreateLoopbackStreamHelper(
    ForwardingAudioStreamFactory::Core* factory,
    AudioStreamBroker::LoopbackSource* loopback_source,
    const media::AudioParameters& params,
    uint32_t total_segments,
    mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
        client_remote) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const bool mute_source = true;
  factory->CreateLoopbackStream(-1, -1, loopback_source, params, total_segments,
                                mute_source, std::move(client_remote));
}

void CreateSystemWideLoopbackStreamHelper(
    ForwardingAudioStreamFactory::Core* factory,
    const media::AudioParameters& params,
    uint32_t total_segments,
    mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
        client_remote) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const bool enable_agc = false;
  factory->CreateInputStream(
      -1, -1, media::AudioDeviceDescription::kLoopbackWithMuteDeviceId, params,
      total_segments, enable_agc, nullptr /* processing_config */,
      std::move(client_remote));
}

}  // namespace

InProcessAudioLoopbackStreamCreator::InProcessAudioLoopbackStreamCreator()
    : factory_(nullptr,
               BrowserMainLoop::GetInstance()
                   ? static_cast<media::UserInputMonitorBase*>(
                         BrowserMainLoop::GetInstance()->user_input_monitor())
                   : nullptr,
               content::GetSystemConnector()->Clone(),
               AudioStreamBrokerFactory::CreateImpl()) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

InProcessAudioLoopbackStreamCreator::~InProcessAudioLoopbackStreamCreator() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void InProcessAudioLoopbackStreamCreator::CreateLoopbackStream(
    WebContents* loopback_source,
    const media::AudioParameters& params,
    uint32_t total_segments,
    const StreamCreatedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient> client;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<StreamCreatedCallbackAdapter>(callback),
      client.InitWithNewPipeAndPassReceiver());
  // Deletion of factory_.core() is posted to the IO thread when |factory_| is
  // destroyed, so Unretained is safe below.
  if (loopback_source) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&CreateLoopbackStreamHelper, factory_.core(),
                                  static_cast<WebContentsImpl*>(loopback_source)
                                      ->GetAudioStreamFactory()
                                      ->core(),
                                  params, total_segments, std::move(client)));
    return;
  }
  // A null |frame_of_source_web_contents| requests system-wide loopback.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&CreateSystemWideLoopbackStreamHelper, factory_.core(),
                     params, total_segments, std::move(client)));
}

}  // namespace content
