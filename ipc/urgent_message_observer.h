// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_URGENT_MESSAGE_OBSERVER_H_
#define IPC_URGENT_MESSAGE_OBSERVER_H_

namespace IPC {

// Interface for observing events related to urgent messages.
class UrgentMessageObserver {
 public:
  virtual ~UrgentMessageObserver() = default;

  // Called on the IPC thread when an urgent message is received.
  virtual void OnUrgentMessageReceived() = 0;

  // Called when an urgent message task has either run or failed to run. When
  // the IPC method is successfully invoked, this callback runs on the same
  // thread as the IPC method, after the IPC method runs. If the IPC method
  // doesn't run, e.g. if the target task runner's queue has been shut down or
  // the interface is closed, the callback can run on either the target thread
  // or IPC thread.
  virtual void OnUrgentMessageProcessed() = 0;
};

}  // namespace IPC

#endif  // IPC_URGENT_MESSAGE_OBSERVER_H_
