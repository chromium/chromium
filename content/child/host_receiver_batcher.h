// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_HOST_RECEIVER_BATCHER_H_
#define CONTENT_CHILD_HOST_RECEIVER_BATCHER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

namespace content {

// Coalesces GenericPendingReceiver bind requests so a startup burst can be sent
// to the browser as a single batched IPC, instead of one IPC per receiver.
//
// Thread-safe: AddReceiver() may be called from any thread. Buffered receivers
// are flushed on the sequence that owns `flush_runner`, and the SendCallback is
// always invoked there. Only receivers whose interface is used ASYNCHRONOUSLY
// may be batched: because the flush is deferred, a synchronous call on a
// not-yet-flushed interface would hang. See
// ChildThread::BindHostReceiverBatched() for the contract enforced at the call
// site.
class CONTENT_EXPORT HostReceiverBatcher {
 public:
  // Invoked on the flush sequence with the coalesced receivers to send.
  using SendCallback =
      base::RepeatingCallback<void(std::vector<mojo::GenericPendingReceiver>)>;

  HostReceiverBatcher(SendCallback send_callback,
                      scoped_refptr<base::SequencedTaskRunner> flush_runner);
  HostReceiverBatcher(const HostReceiverBatcher&) = delete;
  HostReceiverBatcher& operator=(const HostReceiverBatcher&) = delete;
  ~HostReceiverBatcher();

  // Buffers `receiver` and schedules a flush if one isn't already pending.
  // Callable from any thread.
  void AddReceiver(mojo::GenericPendingReceiver receiver);

  // Drops any buffered receivers without sending them (e.g. on disconnect).
  void Clear();

 private:
  void Flush();

  const SendCallback send_callback_;
  const scoped_refptr<base::SequencedTaskRunner> flush_runner_;

  base::Lock lock_;
  std::vector<mojo::GenericPendingReceiver> pending_ GUARDED_BY(lock_);
  bool flush_scheduled_ GUARDED_BY(lock_) = false;

  // Cached at construction (on the flush sequence) so AddReceiver() can post
  // from any thread without calling GetWeakPtr() off-sequence.
  base::WeakPtr<HostReceiverBatcher> weak_this_;
  base::WeakPtrFactory<HostReceiverBatcher> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_CHILD_HOST_RECEIVER_BATCHER_H_
