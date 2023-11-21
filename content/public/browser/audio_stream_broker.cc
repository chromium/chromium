// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/audio_stream_broker.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

AudioStreamBroker::LoopbackSink::LoopbackSink() = default;
AudioStreamBroker::LoopbackSink::~LoopbackSink() = default;

AudioStreamBroker::LoopbackSource::LoopbackSource() = default;
AudioStreamBroker::LoopbackSource::~LoopbackSource() = default;

AudioStreamBroker::AudioStreamBroker(int render_process_id, int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}
AudioStreamBroker::~AudioStreamBroker() = default;

void AudioStreamBroker::NotifyHostOfStartedStream() {
  auto impl = [](int render_process_id, int render_frame_id) {
    if (auto* host =
            RenderFrameHostImpl::FromID(render_process_id, render_frame_id)) {
      host->OnMediaStreamAdded();
    }
  };
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(impl, render_process_id(), render_frame_id()));
}

void AudioStreamBroker::NotifyHostOfStoppedStream() {
  auto impl = [](int render_process_id, int render_frame_id) {
    if (auto* host =
            RenderFrameHostImpl::FromID(render_process_id, render_frame_id)) {
      host->OnMediaStreamRemoved();
    }
  };
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(impl, render_process_id(), render_frame_id()));
}

AudioStreamBrokerFactory::AudioStreamBrokerFactory() = default;
AudioStreamBrokerFactory::~AudioStreamBrokerFactory() = default;

}  // namespace content
