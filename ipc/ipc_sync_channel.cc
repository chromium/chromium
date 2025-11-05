// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_channel.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/param_traits_macros.h"
#include "mojo/public/cpp/bindings/sync_event_watcher.h"

using base::WaitableEvent;

namespace IPC {

namespace {

// Holds a pointer to the per-thread ReceivedSyncMsgQueue object.
constinit thread_local SyncChannel::ReceivedSyncMsgQueue* received_queue =
    nullptr;

}  // namespace

// Currently, this class remains to manage a shared waitable event on
// a thread across a number of sync channels on that thread.
class SyncChannel::ReceivedSyncMsgQueue :
    public base::RefCountedThreadSafe<ReceivedSyncMsgQueue> {
 public:
  // Returns the ReceivedSyncMsgQueue instance for this thread, creating one
  // if necessary.  Call RemoveContext on the same thread when done.
  static ReceivedSyncMsgQueue* AddContext() {
    // We want one ReceivedSyncMsgQueue per listener thread (i.e. since multiple
    // SyncChannel objects can block the same thread).
    if (!received_queue) {
      received_queue = new ReceivedSyncMsgQueue();
    }
    ++received_queue->listener_count_;
    return received_queue;
  }

  // SyncChannel calls this in its destructor.
  void RemoveContext(SyncContext* context) {
    base::AutoLock auto_lock(message_lock_);

    if (--listener_count_ == 0) {
      DCHECK(received_queue);
      received_queue = nullptr;
      sync_dispatch_watcher_.reset();
    }
  }

  base::WaitableEvent* dispatch_event() { return &dispatch_event_; }

 private:
  friend class base::RefCountedThreadSafe<ReceivedSyncMsgQueue>;

  // See the comment in SyncChannel::SyncChannel for why this event is created
  // as manual reset.
  ReceivedSyncMsgQueue()
      : dispatch_event_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        sync_dispatch_watcher_(std::make_unique<mojo::SyncEventWatcher>(
            &dispatch_event_,
            base::BindRepeating(&ReceivedSyncMsgQueue::OnDispatchEventReady,
                                base::Unretained(this)))) {
    sync_dispatch_watcher_->AllowWokenUpBySyncWatchOnSameThread();
  }

  ~ReceivedSyncMsgQueue() = default;

  void OnDispatchEventReady() {}

  // Signaled when we get a synchronous message that we must respond to, as the
  // sender needs its reply before it can reply to our original synchronous
  // message.
  base::WaitableEvent dispatch_event_;
  base::Lock message_lock_;
  int listener_count_ = 0;

  // Watches |dispatch_event_| during all sync handle watches on this thread.
  std::unique_ptr<mojo::SyncEventWatcher> sync_dispatch_watcher_;
};

SyncChannel::SyncContext::SyncContext(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
    WaitableEvent* shutdown_event)
    : ChannelProxy::Context(listener, ipc_task_runner, listener_task_runner),
      received_sync_msgs_(ReceivedSyncMsgQueue::AddContext()),
      shutdown_event_(shutdown_event) {}

SyncChannel::SyncContext::~SyncContext() = default;

base::WaitableEvent* SyncChannel::SyncContext::GetDispatchEvent() {
  return received_sync_msgs_->dispatch_event();
}

void SyncChannel::SyncContext::Clear() {
  received_sync_msgs_->RemoveContext(this);
  Context::Clear();
}

void SyncChannel::SyncContext::OnChannelError() {
  shutdown_watcher_.StopWatching();
  Context::OnChannelError();
}

void SyncChannel::SyncContext::OnChannelOpened() {
  if (shutdown_event_) {
    shutdown_watcher_.StartWatching(
        shutdown_event_,
        base::BindOnce(&SyncChannel::SyncContext::OnShutdownEventSignaled,
                       base::Unretained(this)),
        base::SequencedTaskRunner::GetCurrentDefault());
  }
  Context::OnChannelOpened();
}

void SyncChannel::SyncContext::OnChannelClosed() {
  shutdown_watcher_.StopWatching();
  Context::OnChannelClosed();
}

void SyncChannel::SyncContext::OnShutdownEventSignaled(WaitableEvent* event) {
  DCHECK_EQ(event, shutdown_event_);
}

// static
std::unique_ptr<SyncChannel> SyncChannel::Create(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
    WaitableEvent* shutdown_event) {
  return base::WrapUnique(new SyncChannel(
      listener, ipc_task_runner, listener_task_runner, shutdown_event));
}

SyncChannel::SyncChannel(
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& listener_task_runner,
    WaitableEvent* shutdown_event)
    : ChannelProxy(new SyncContext(listener,
                                   ipc_task_runner,
                                   listener_task_runner,
                                   shutdown_event)),
      sync_handle_registry_(mojo::SyncHandleRegistry::current()) {
  // The current (listener) thread must be distinct from the IPC thread, or else
  // sending synchronous messages will deadlock.
  DCHECK_NE(ipc_task_runner.get(),
            base::SingleThreadTaskRunner::GetCurrentDefault().get());
  StartWatching();
}

SyncChannel::~SyncChannel() = default;

void SyncChannel::OnDispatchEventSignaled(base::WaitableEvent* event) {
  DCHECK_EQ(sync_context()->GetDispatchEvent(), event);
  sync_context()->GetDispatchEvent()->Reset();
  StartWatching();
}

void SyncChannel::StartWatching() {
  // |dispatch_watcher_| watches the event asynchronously, only dispatching
  // messages once the listener thread is unblocked and pumping its task queue.
  // The ReceivedSyncMsgQueue also watches this event and may dispatch
  // immediately if woken up by a message which it's allowed to dispatch.
  dispatch_watcher_.StartWatching(
      sync_context()->GetDispatchEvent(),
      base::BindOnce(&SyncChannel::OnDispatchEventSignaled,
                     base::Unretained(this)),
      sync_context()->listener_task_runner());
}

}  // namespace IPC
