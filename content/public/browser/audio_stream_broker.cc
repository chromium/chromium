// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/audio_stream_broker.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

AudioStreamBroker::LoopbackSink::LoopbackSink() = default;
AudioStreamBroker::LoopbackSink::~LoopbackSink() = default;

AudioStreamBroker::LoopbackSource::LoopbackSource() = default;
AudioStreamBroker::LoopbackSource::~LoopbackSource() = default;

AudioStreamBroker::AudioStreamBroker(int render_process_id, int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}
AudioStreamBroker::~AudioStreamBroker() = default;

// static
void AudioStreamBroker::NotifyProcessHostOfStartedStream(
    int render_process_id) {
  auto impl = [](int id) {
    if (auto* process_host = RenderProcessHost::FromID(id))
      process_host->OnMediaStreamAdded();
  };
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                      base::BindOnce(impl, render_process_id));
}

// static
void AudioStreamBroker::NotifyProcessHostOfStoppedStream(
    int render_process_id) {
  auto impl = [](int id) {
    if (auto* process_host = RenderProcessHost::FromID(id))
      process_host->OnMediaStreamRemoved();
  };
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                      base::BindOnce(impl, render_process_id));
}

AudioStreamBrokerFactory::AudioStreamBrokerFactory() = default;
AudioStreamBrokerFactory::~AudioStreamBrokerFactory() = default;

}  // namespace content
