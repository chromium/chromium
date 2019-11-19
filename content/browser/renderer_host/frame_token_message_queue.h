// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_FRAME_TOKEN_MESSAGE_QUEUE_H_
#define CONTENT_BROWSER_RENDERER_HOST_FRAME_TOKEN_MESSAGE_QUEUE_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "content/common/content_export.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

// The Renderer sends various messages which are not to be processed until after
// their associated frame has been processed. These messages are provided with a
// FrameToken to be used for synchronizing.
//
// Viz processes the frames, after which it notifies the Browser of which
// FrameToken has completed processing.
//
// This enqueues all IPC::Messages associated with a FrameToken.
//
// Additionally other callbacks can be enqueued with
// EnqueueOrRunFrameTokenCallback.
//
// Upon receipt of DidProcessFrame all IPC::Messages associated with the
// provided FrameToken are then dispatched, and all enqueued callbacks are ran.
class CONTENT_EXPORT FrameTokenMessageQueue {
 public:
  // Notified of errors in processing messages, as well as of the actual
  // enqueued IPC::Messages which need processing.
  class Client {
   public:
    ~Client() {}

    // Notified when an invalid frame token was received.
    virtual void OnInvalidFrameToken(uint32_t frame_token) = 0;

    // Called when there are dispatching errors in OnMessageReceived().
    virtual void OnMessageDispatchError(const IPC::Message& message) = 0;

    // Process the enqueued message.
    virtual void OnProcessSwapMessage(const IPC::Message& message) = 0;
  };
  FrameTokenMessageQueue();
  virtual ~FrameTokenMessageQueue();

  // Initializes this instance. This should always be the first method called
  // to initialize state.
  void Init(Client* client);

  // Signals that a frame with token |frame_token| was finished processing. If
  // there are any queued messages belonging to it, they will be processed.
  void DidProcessFrame(uint32_t frame_token);

  // Enqueues |callback| to be called upon the arrival of |frame_token| in
  // DidProcessFrame. However if |frame_token| has already arrived |callback| is
  // ran immediately.
  void EnqueueOrRunFrameTokenCallback(uint32_t frame_token,
                                      base::OnceClosure callback);

  // Enqueues the swap messages.
  void OnFrameSwapMessagesReceived(uint32_t frame_token,
                                   std::vector<IPC::Message> messages);

  // Called when the renderer process is gone. This will reset our state to be
  // consistent incase a new renderer is created.
  void Reset();

  size_t size() const { return callback_map_.size(); }

 protected:
  // Once both the frame and its swap messages arrive, we call this method to
  // process the messages. Virtual for tests.
  virtual void ProcessSwapMessages(std::vector<IPC::Message> messages);

 private:
  // Not owned.
  Client* client_ = nullptr;

  // Last non-zero frame token received from the renderer. Any swap messsages
  // having a token less than or equal to this value will be processed.
  uint32_t last_received_frame_token_ = 0;

  // Map of all callbacks for which their corresponding frame have not arrived.
  // Sorted by frame token.
  std::multimap<uint32_t, base::OnceClosure> callback_map_;

  DISALLOW_COPY_AND_ASSIGN(FrameTokenMessageQueue);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_FRAME_TOKEN_MESSAGE_QUEUE_H_
