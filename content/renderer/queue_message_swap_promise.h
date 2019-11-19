// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_QUEUE_MESSAGE_SWAP_PROMISE_H_
#define CONTENT_RENDERER_QUEUE_MESSAGE_SWAP_PROMISE_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "cc/trees/swap_promise.h"

namespace IPC {
class SyncMessageFilter;
}

namespace content {

class FrameSwapMessageQueue;

class QueueMessageSwapPromise : public cc::SwapPromise {
 public:
  QueueMessageSwapPromise(scoped_refptr<IPC::SyncMessageFilter> message_sender,
                          scoped_refptr<FrameSwapMessageQueue> message_queue,
                          int source_frame_number);

  ~QueueMessageSwapPromise() override;

  void DidActivate() override;
  void WillSwap(viz::CompositorFrameMetadata* metadata) override;
  void DidSwap() override;
  DidNotSwapAction DidNotSwap(DidNotSwapReason reason) override;

  int64_t TraceId() const override;

 private:
  scoped_refptr<IPC::SyncMessageFilter> message_sender_;
  scoped_refptr<content::FrameSwapMessageQueue> message_queue_;
  int source_frame_number_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_QUEUE_MESSAGE_SWAP_PROMISE_H_
