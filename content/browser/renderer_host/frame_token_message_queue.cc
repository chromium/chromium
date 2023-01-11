// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_token_message_queue.h"

#include "base/functional/bind.h"

namespace content {

FrameTokenMessageQueue::FrameTokenMessageQueue() = default;

FrameTokenMessageQueue::~FrameTokenMessageQueue() = default;

void FrameTokenMessageQueue::Init(Client* client) {
  client_ = client;
}

void FrameTokenMessageQueue::DidProcessFrame(uint32_t frame_token,
                                             base::TimeTicks activation_time) {
  // The queue will be cleared if the Renderer has been Reset. Do not enforce
  // token order, as ACKs for old frames may still be in flight from Viz.
  if (callback_map_.empty()) {
    last_received_frame_token_ = frame_token;
    last_received_activation_time_ = activation_time;
    return;
  }

  // Frame tokens always increase. However when a Reset occurs old tokens can
  // arrive. Do not enforce token order if we are seeing the ACK for the
  // previous frame.
  // TODO(jonross): we should consider updating LocalSurfaceId to also track
  // frame_token. So that we could properly differentiate between origins of
  // frame. As we cannot enforce ordering between Reset Renderers.
  if ((frame_token <= last_received_frame_token_) &&
      !(last_received_frame_token_reset_ &&
        last_received_frame_token_reset_ != frame_token)) {
    client_->OnInvalidFrameToken(frame_token);
    return;
  }

  last_received_frame_token_ = frame_token;
  last_received_activation_time_ = activation_time;

  // Gets the first callback associated with a token after |frame_token| or
  // callback_map_.end().
  auto upper_bound = callback_map_.upper_bound(frame_token);

  // std::multimap already sorts on keys, so this will process all enqueued
  // messages up to the current frame token.
  for (auto it = callback_map_.begin(); it != upper_bound; ++it)
    std::move(it->second).Run(activation_time);

  // Clear all callbacks up to the current frame token.
  callback_map_.erase(callback_map_.begin(), upper_bound);
}

void FrameTokenMessageQueue::EnqueueOrRunFrameTokenCallback(
    uint32_t frame_token,
    base::OnceCallback<void(base::TimeTicks)> callback) {
  if (frame_token <= last_received_frame_token_) {
    std::move(callback).Run(last_received_activation_time_);
    return;
  }
  callback_map_.insert(std::make_pair(frame_token, std::move(callback)));
}

void FrameTokenMessageQueue::Reset() {
  last_received_frame_token_reset_ = last_received_frame_token_;
  last_received_frame_token_ = 0;
  callback_map_.clear();
}

}  // namespace content
