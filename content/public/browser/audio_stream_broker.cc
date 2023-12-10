// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/audio_stream_broker.h"

namespace content {

AudioStreamBroker::LoopbackSink::LoopbackSink() = default;
AudioStreamBroker::LoopbackSink::~LoopbackSink() = default;

AudioStreamBroker::LoopbackSource::LoopbackSource() = default;
AudioStreamBroker::LoopbackSource::~LoopbackSource() = default;

AudioStreamBroker::AudioStreamBroker(int render_process_id, int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}
AudioStreamBroker::~AudioStreamBroker() = default;

AudioStreamBrokerFactory::AudioStreamBrokerFactory() = default;
AudioStreamBrokerFactory::~AudioStreamBrokerFactory() = default;

}  // namespace content
