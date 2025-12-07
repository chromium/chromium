// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/common/frame_consumer_wrapper.h"

#include "base/notreached.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/session_config.h"

namespace remoting {

namespace {
using protocol::SessionConfig;
}

FrameConsumerWrapper::FrameConsumerWrapper(protocol::FrameConsumer* consumer)
    : consumer_(consumer) {
  CHECK(consumer_);
}

FrameConsumerWrapper::~FrameConsumerWrapper() = default;

bool FrameConsumerWrapper::Initialize(
    const ClientContext& client_context,
    protocol::FrameStatsConsumer* stats_consumer) {
  // FrameConsumerWrapper::Initialize() is not called for WebRTC.
  NOTREACHED();
}

void FrameConsumerWrapper::OnSessionConfig(
    const protocol::SessionConfig& config) {
  // FrameConsumerWrapper::OnSessionConfig() is not called for WebRTC.
  NOTREACHED();
}

protocol::VideoStub* FrameConsumerWrapper::GetVideoStub() {
  return nullptr;
}

protocol::FrameConsumer* FrameConsumerWrapper::GetFrameConsumer() {
  return consumer_;
}

protocol::FrameStatsConsumer* FrameConsumerWrapper::GetFrameStatsConsumer() {
  return nullptr;
}

}  // namespace remoting
