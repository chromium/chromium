// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_SYNC_CHANNEL_H_
#define IPC_IPC_SYNC_CHANNEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/single_thread_task_runner.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace mojo {
class SyncHandleRegistry;
}

namespace IPC {

// This is similar to ChannelProxy.
//
// Overview of how the sync channel works
// --------------------------------------
// When the sending thread sends a synchronous message, we create a bunch
// of tracking info (created in Send, stored in the PendingSyncMsg
// structure) associated with the message that we identify by the unique
// "MessageId" on the SyncMessage. Among the things we save is the
// "Deserializer" which is provided by the sync message. This object is in
// charge of reading the parameters from the reply message and putting them in
// the output variables provided by its caller.
//
// The info gets stashed in a queue since we could have a nested stack of sync
// messages (each side could send sync messages in response to sync messages,
// so it works like calling a function). The message is sent to the I/O thread
// for dispatch and the original thread blocks waiting for the reply.
//
// SyncContext maintains the queue in a threadsafe way and listens for replies
// on the I/O thread. When a reply comes in that matches one of the messages
// it's looking for (using the unique message ID), it will execute the
// deserializer stashed from before, and unblock the original thread.
//
//
// Significant complexity results from the fact that messages are still coming
// in while the original thread is blocked. Normal async messages are queued
// and dispatched after the blocking call is complete. Sync messages must
// be dispatched in a reentrant manner to avoid deadlock.
//
//
// Note that care must be taken that the lifetime of the ipc_thread argument
// is more than this object.  If the message loop goes away while this object
// is running and it's used to send a message, then it will use the invalid
// message loop pointer to proxy it to the ipc thread.
class COMPONENT_EXPORT(IPC) SyncChannel : public ChannelProxy {
 public:
  class ReceivedSyncMsgQueue;

  // Creates an uninitialized sync channel. Call ChannelProxy::Init() to
  // initialize the channel after creation.
  static std::unique_ptr<SyncChannel> Create(
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
      base::WaitableEvent* shutdown_event);

  SyncChannel(const SyncChannel&) = delete;
  SyncChannel& operator=(const SyncChannel&) = delete;

  ~SyncChannel() override;

 protected:
  friend class ReceivedSyncMsgQueue;

  // SyncContext holds the per object data for SyncChannel, so that SyncChannel
  // can be deleted while it's being used in a different thread.  See
  // ChannelProxy::Context for more information.
  class SyncContext : public Context {
   public:
    SyncContext(
        Listener* listener,
        const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
        const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
        base::WaitableEvent* shutdown_event);

    // Returns a Mojo Event that signals when an incoming message that's not the
    // pending reply needs to get dispatched (by calling DispatchMessages.)
    base::WaitableEvent* GetDispatchEvent();

   private:
    ~SyncContext() override;
    // ChannelProxy methods that we override.

    // Called on the listener thread.
    void Clear() override;

    // Called on the IPC thread.
    void OnChannelError() override;
    void OnChannelOpened() override;
    void OnChannelClosed() override;

    void OnShutdownEventSignaled(base::WaitableEvent* event);

    scoped_refptr<ReceivedSyncMsgQueue> received_sync_msgs_;
    raw_ptr<base::WaitableEvent, AcrossTasksDanglingUntriaged> shutdown_event_;
    base::WaitableEventWatcher shutdown_watcher_;
    base::WaitableEventWatcher::EventCallback shutdown_watcher_callback_;
  };

 private:
  SyncChannel(
      Listener* listener,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
      base::WaitableEvent* shutdown_event);

  void OnDispatchEventSignaled(base::WaitableEvent* event);

  SyncContext* sync_context() {
    return reinterpret_cast<SyncContext*>(context());
  }

  // Starts the dispatch watcher.
  void StartWatching();

  scoped_refptr<mojo::SyncHandleRegistry> sync_handle_registry_;

  // Used to signal events between the IPC and listener threads.
  base::WaitableEventWatcher dispatch_watcher_;
};

}  // namespace IPC

#endif  // IPC_IPC_SYNC_CHANNEL_H_
