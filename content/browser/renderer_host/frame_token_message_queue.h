// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_TOKEN_MESSAGE_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_TOKEN_MESSAGE_QUEUE_H_

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// The Renderer sends various messages which are not to be processed until after
// their associated frame has been processed. These messages are provided with a
// FrameToken to be used for synchronizing.
//
// Viz processes the frames, after which it notifies the Browser of which
// FrameToken has completed processing.
//
// Non-IPC callbacks can be enqueued with EnqueueOrRunFrameTokenCallback.
//
// Upon receipt of DidProcessFrame all Messages associated with the provided
// FrameToken are then dispatched, and all enqueued callbacks are ran.
class CONTENT_EXPORT FrameTokenMessageQueue {
 public:
  // Notified of errors in processing messages.
  class Client {
   public:
    ~Client() {}

    // Notified when an invalid frame token was received.
    virtual void OnInvalidFrameToken(uint32_t frame_token) = 0;
  };
  FrameTokenMessageQueue();

  FrameTokenMessageQueue(const FrameTokenMessageQueue&) = delete;
  FrameTokenMessageQueue& operator=(const FrameTokenMessageQueue&) = delete;

  virtual ~FrameTokenMessageQueue();

  // Initializes this instance. This should always be the first method called
  // to initialize state.
  void Init(Client* client);

  // Signals that a frame with token |frame_token| was finished processing. If
  // there are any queued messages belonging to it, they will be processed.
  void DidProcessFrame(uint32_t frame_token, base::TimeTicks activation_time);

  // Enqueues |callback| to be called upon the arrival of |frame_token| in
  // DidProcessFrame. However if |frame_token| has already arrived |callback| is
  // ran immediately. The |callback| is provided the time for which the frame
  // was activated.
  void EnqueueOrRunFrameTokenCallback(
      uint32_t frame_token,
      base::OnceCallback<void(base::TimeTicks)> callback);

  // Called when the renderer process is gone. This will reset our state to be
  // consistent incase a new renderer is created.
  void Reset();

  size_t size() const { return callback_map_.size(); }

 private:
  // Not owned.
  raw_ptr<Client, DanglingUntriaged> client_ = nullptr;

  // Last non-zero frame token received from the renderer. Any swap messsages
  // having a token less than or equal to this value will be processed.
  uint32_t last_received_frame_token_ = 0;
  base::TimeTicks last_received_activation_time_;

  // Map of all callbacks for which their corresponding frame have not arrived.
  // Sorted by frame token.
  std::multimap<uint32_t, base::OnceCallback<void(base::TimeTicks)>>
      callback_map_;

  // The frame token last seen when Reset() is called. To determine if we are
  // getting delayed frame acknowledgements after a reset.
  uint32_t last_received_frame_token_reset_ = 0u;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_TOKEN_MESSAGE_QUEUE_H_
