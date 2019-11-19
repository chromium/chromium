// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_loopback_stream_broker.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace content {

AudioLoopbackStreamBroker::AudioLoopbackStreamBroker(
    int render_process_id,
    int render_frame_id,
    AudioStreamBroker::LoopbackSource* source,
    const media::AudioParameters& params,
    uint32_t shared_memory_count,
    bool mute_source,
    AudioStreamBroker::DeleterCallback deleter,
    mojo::PendingRemote<mojom::RendererAudioInputStreamFactoryClient>
        renderer_factory_client)
    : AudioStreamBroker(render_process_id, render_frame_id),
      source_(source),
      params_(params),
      shared_memory_count_(shared_memory_count),
      deleter_(std::move(deleter)),
      renderer_factory_client_(std::move(renderer_factory_client)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(source_);
  DCHECK(renderer_factory_client_);
  DCHECK(deleter_);

  if (mute_source) {
    muter_.emplace(source_->GetGroupID());
  }

  // Unretained is safe because |this| owns |renderer_factory_client_|.
  renderer_factory_client_.set_disconnect_handler(base::BindOnce(
      &AudioLoopbackStreamBroker::Cleanup, base::Unretained(this)));

  // Notify the source that we are capturing from it.
  source_->AddLoopbackSink(this);

  NotifyProcessHostOfStartedStream(render_process_id);
}

AudioLoopbackStreamBroker::~AudioLoopbackStreamBroker() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (source_)
    source_->RemoveLoopbackSink(this);

  NotifyProcessHostOfStoppedStream(render_process_id());
}

void AudioLoopbackStreamBroker::CreateStream(
    audio::mojom::StreamFactory* factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!observer_receiver_.is_bound());
  DCHECK(!client_receiver_);
  DCHECK(source_);

  if (muter_)  // Mute the source.
    muter_->Connect(factory);

  mojo::PendingRemote<media::mojom::AudioInputStreamClient> client;
  client_receiver_ = client.InitWithNewPipeAndPassReceiver();

  mojo::PendingRemote<media::mojom::AudioInputStream> stream;
  mojo::PendingReceiver<media::mojom::AudioInputStream> stream_receiver =
      stream.InitWithNewPipeAndPassReceiver();

  factory->CreateLoopbackStream(
      std::move(stream_receiver), std::move(client),
      observer_receiver_.BindNewPipeAndPassRemote(), params_,
      shared_memory_count_, source_->GetGroupID(),
      base::BindOnce(&AudioLoopbackStreamBroker::StreamCreated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(stream)));

  // Unretained is safe because |this| owns |observer_receiver_|.
  observer_receiver_.set_disconnect_handler(base::BindOnce(
      &AudioLoopbackStreamBroker::Cleanup, base::Unretained(this)));
}

void AudioLoopbackStreamBroker::OnSourceGone() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // No further access to |source_| is allowed.
  source_ = nullptr;
  Cleanup();
}

void AudioLoopbackStreamBroker::DidStartRecording() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void AudioLoopbackStreamBroker::StreamCreated(
    mojo::PendingRemote<media::mojom::AudioInputStream> stream,
    media::mojom::ReadOnlyAudioDataPipePtr data_pipe) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!data_pipe) {
    Cleanup();
    return;
  }

  DCHECK(renderer_factory_client_);
  renderer_factory_client_->StreamCreated(
      std::move(stream), std::move(client_receiver_), std::move(data_pipe),
      false /* |initially_muted|: Loopback streams are never muted. */,
      base::nullopt /* |stream_id|: Loopback streams don't have ids */);
}

void AudioLoopbackStreamBroker::Cleanup() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::move(deleter_).Run(this);
}

}  // namespace content
