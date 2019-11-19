// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_token_message_queue.h"

#include "base/bind.h"
#include "ipc/ipc_message.h"

namespace content {

FrameTokenMessageQueue::FrameTokenMessageQueue() = default;

FrameTokenMessageQueue::~FrameTokenMessageQueue() = default;

void FrameTokenMessageQueue::Init(Client* client) {
  client_ = client;
}

void FrameTokenMessageQueue::DidProcessFrame(uint32_t frame_token) {
  // Frame tokens always increase.
  if (frame_token <= last_received_frame_token_) {
    client_->OnInvalidFrameToken(frame_token);
    return;
  }

  last_received_frame_token_ = frame_token;

  // Gets the first callback associated with a token after |frame_token| or
  // callback_map_.end().
  auto upper_bound = callback_map_.upper_bound(frame_token);

  // std::multimap already sorts on keys, so this will process all enqueued
  // messages up to the current frame token.
  for (auto it = callback_map_.begin(); it != upper_bound; ++it)
    std::move(it->second).Run();

  // Clear all callbacks up to the current frame token.
  callback_map_.erase(callback_map_.begin(), upper_bound);
}

void FrameTokenMessageQueue::EnqueueOrRunFrameTokenCallback(
    uint32_t frame_token,
    base::OnceClosure callback) {
  // Zero token is invalid.
  if (!frame_token) {
    client_->OnInvalidFrameToken(frame_token);
    return;
  }

  if (frame_token <= last_received_frame_token_) {
    std::move(callback).Run();
    return;
  }
  callback_map_.insert(std::make_pair(frame_token, std::move(callback)));
}

void FrameTokenMessageQueue::OnFrameSwapMessagesReceived(
    uint32_t frame_token,
    std::vector<IPC::Message> messages) {
  EnqueueOrRunFrameTokenCallback(
      frame_token, base::BindOnce(&FrameTokenMessageQueue::ProcessSwapMessages,
                                  base::Unretained(this), std::move(messages)));
}

void FrameTokenMessageQueue::Reset() {
  last_received_frame_token_ = 0;
  callback_map_.clear();
}

void FrameTokenMessageQueue::ProcessSwapMessages(
    std::vector<IPC::Message> messages) {
  for (const IPC::Message& i : messages) {
    client_->OnProcessSwapMessage(i);
    if (i.dispatch_error())
      client_->OnMessageDispatchError(i);
  }
}

}  // namespace content
