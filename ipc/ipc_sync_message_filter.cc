// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_sync_message_filter.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_sync_message.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/sync_handle_registry.h"

namespace IPC {

namespace {

// A generic callback used when watching handles synchronously. Sets |*signal|
// to true.
void OnEventReady(bool* signal) {
  *signal = true;
}

}  // namespace

bool SyncMessageFilter::Send(Message* message) {
  if (!message->is_sync()) {
    {
      base::AutoLock auto_lock(lock_);
      if (!io_task_runner_.get()) {
        pending_messages_.emplace_back(base::WrapUnique(message));
        return true;
      }
    }
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SyncMessageFilter::SendOnIOThread, this, message));
    return true;
  }

  auto owned_event = std::make_unique<base::WaitableEvent>(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent* done_event = owned_event.get();
  PendingSyncMsg pending_message(
      SyncMessage::GetMessageId(*message),
      static_cast<SyncMessage*>(message)->TakeReplyDeserializer(),
      std::move(owned_event));

  {
    base::AutoLock auto_lock(lock_);
    // Can't use this class on the main thread or else it can lead to deadlocks.
    // Also by definition, can't use this on IO thread since we're blocking it.
    if (base::SingleThreadTaskRunner::HasCurrentDefault()) {
      DCHECK(base::SingleThreadTaskRunner::GetCurrentDefault() !=
             listener_task_runner_);
      DCHECK(base::SingleThreadTaskRunner::GetCurrentDefault() !=
             io_task_runner_);
    }
    pending_sync_messages_.insert(&pending_message);

    if (io_task_runner_.get()) {
      io_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&SyncMessageFilter::SendOnIOThread, this, message));
    } else {
      pending_messages_.emplace_back(base::WrapUnique(message));
    }
  }

  {
    bool done = false;
    bool shutdown = false;
    scoped_refptr<mojo::SyncHandleRegistry> registry =
        mojo::SyncHandleRegistry::current();
    mojo::SyncHandleRegistry::EventCallbackSubscription shutdown_subscription =
        registry->RegisterEvent(shutdown_event_,
                                base::BindRepeating(&OnEventReady, &shutdown));
    mojo::SyncHandleRegistry::EventCallbackSubscription done_subscription =
        registry->RegisterEvent(done_event,
                                base::BindRepeating(&OnEventReady, &done));

    const bool* stop_flags[] = {&done, &shutdown};
    registry->Wait(stop_flags, 2);
    if (done) {
      TRACE_EVENT_WITH_FLOW0("toplevel.flow", "SyncMessageFilter::Send",
                             done_event, TRACE_EVENT_FLAG_FLOW_IN);
    }
  }

  {
    base::AutoLock auto_lock(lock_);
    pending_sync_messages_.erase(&pending_message);
  }

  return pending_message.send_result;
}

void SyncMessageFilter::OnFilterAdded(Channel* channel) {
  std::vector<std::unique_ptr<Message>> pending_messages;
  {
    base::AutoLock auto_lock(lock_);
    channel_ = channel;

    io_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    std::swap(pending_messages_, pending_messages);
  }
  for (auto& msg : pending_messages)
    SendOnIOThread(msg.release());
}

void SyncMessageFilter::OnChannelError() {
  base::AutoLock auto_lock(lock_);
  channel_ = nullptr;
  SignalAllEvents();
}

void SyncMessageFilter::OnChannelClosing() {
  base::AutoLock auto_lock(lock_);
  channel_ = nullptr;
  SignalAllEvents();
}

bool SyncMessageFilter::OnMessageReceived(const Message& message) {
  base::AutoLock auto_lock(lock_);
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    if (SyncMessage::IsMessageReplyTo(message, (*iter)->id)) {
      if (!message.is_reply_error()) {
        (*iter)->send_result =
            (*iter)->deserializer->SerializeOutputParameters(message);
      }
      TRACE_EVENT_WITH_FLOW0(
          "toplevel.flow", "SyncMessageFilter::OnMessageReceived",
          (*iter)->done_event.get(), TRACE_EVENT_FLAG_FLOW_OUT);
      (*iter)->done_event->Signal();
      return true;
    }
  }

  return false;
}

SyncMessageFilter::SyncMessageFilter(base::WaitableEvent* shutdown_event)
    : channel_(nullptr),
      listener_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      shutdown_event_(shutdown_event) {}

SyncMessageFilter::~SyncMessageFilter() = default;

void SyncMessageFilter::SendOnIOThread(Message* message) {
  if (channel_) {
    channel_->Send(message);
    return;
  }

  if (message->is_sync()) {
    // We don't know which thread sent it, but it doesn't matter, just signal
    // them all.
    base::AutoLock auto_lock(lock_);
    SignalAllEvents();
  }

  delete message;
}

void SyncMessageFilter::SignalAllEvents() {
  lock_.AssertAcquired();
  for (PendingSyncMessages::iterator iter = pending_sync_messages_.begin();
       iter != pending_sync_messages_.end(); ++iter) {
    TRACE_EVENT_WITH_FLOW0(
        "toplevel.flow", "SyncMessageFilter::SignalAllEvents",
        (*iter)->done_event.get(), TRACE_EVENT_FLAG_FLOW_OUT);
    (*iter)->done_event->Signal();
  }
}

void SyncMessageFilter::GetRemoteAssociatedInterface(
    mojo::GenericPendingAssociatedReceiver receiver) {
  base::AutoLock auto_lock(lock_);
  DCHECK(io_task_runner_ && io_task_runner_->BelongsToCurrentThread());
  if (!channel_) {
    // Attach the associated interface to a disconnected pipe, so that the
    // associated interface pointer can be used to make calls (which are
    // dropped).
    mojo::AssociateWithDisconnectedPipe(receiver.PassHandle());
    return;
  }

  Channel::AssociatedInterfaceSupport* support =
      channel_->GetAssociatedInterfaceSupport();
  support->GetRemoteAssociatedInterface(std::move(receiver));
}

}  // namespace IPC
