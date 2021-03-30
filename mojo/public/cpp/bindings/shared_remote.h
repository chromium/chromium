// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SHARED_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SHARED_REMOTE_H_

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/lib/thread_safe_forwarder_base.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/bindings/sync_event_watcher.h"

namespace mojo {

// Helper that may be used from any sequence to serialize |Interface| messages
// and forward them elsewhere. In general, prefer `SharedRemote`, but this type
// may be useful when it's necessary to manually manage the lifetime of the
// underlying proxy object which will be used to ultimately send messages.
template <typename Interface>
class ThreadSafeForwarder : public internal::ThreadSafeForwarderBase {
 public:
  using ProxyType = typename Interface::Proxy_;

  // Constructs a ThreadSafeForwarder through which Messages are forwarded to
  // |forward| or |forward_with_responder| by posting to |task_runner|.
  //
  // Any message sent through this forwarding interface will dispatch its reply,
  // if any, back to the sequence which called the corresponding interface
  // method.
  ThreadSafeForwarder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ForwardMessageCallback forward,
      ForwardMessageWithResponderCallback forward_with_responder,
      ForceAsyncSendCallback force_async_send,
      const AssociatedGroup& associated_group)
      : ThreadSafeForwarderBase(std::move(task_runner),
                                std::move(forward),
                                std::move(forward_with_responder),
                                std::move(force_async_send),
                                associated_group),
        proxy_(this) {}

  ~ThreadSafeForwarder() override = default;

  ProxyType& proxy() { return proxy_; }

 private:
  ProxyType proxy_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeForwarder);
};

template <typename Interface>
class SharedRemote;

template <typename RemoteType>
class SharedRemoteBase
    : public base::RefCountedThreadSafe<SharedRemoteBase<RemoteType>> {
 public:
  using InterfaceType = typename RemoteType::InterfaceType;
  using PendingType = typename RemoteType::PendingType;

  InterfaceType* get() { return &forwarder_->proxy(); }
  InterfaceType* operator->() { return get(); }
  InterfaceType& operator*() { return *get(); }

  void set_disconnect_handler(
      base::OnceClosure handler,
      scoped_refptr<base::SequencedTaskRunner> handler_task_runner) {
    wrapper_->set_disconnect_handler(std::move(handler),
                                     std::move(handler_task_runner));
  }

 private:
  friend class base::RefCountedThreadSafe<SharedRemoteBase<RemoteType>>;
  template <typename Interface>
  friend class SharedRemote;
  template <typename Interface>
  friend class SharedAssociatedRemote;

  struct RemoteWrapperDeleter;

  // Helper class which owns a |RemoteType| instance on an appropriate sequence.
  // This is kept alive as long as it's bound within some ThreadSafeForwarder's
  // callbacks.
  class RemoteWrapper
      : public base::RefCountedThreadSafe<RemoteWrapper, RemoteWrapperDeleter> {
   public:
    explicit RemoteWrapper(RemoteType remote)
        : RemoteWrapper(base::SequencedTaskRunnerHandle::Get()) {
      remote_ = std::move(remote);
      associated_group_ = *remote_.internal_state()->associated_group();

      // By default we force all messages to behave as if async within the
      // Remote, as SharedRemote implements its own waiting mechanism to block
      // only the calling thread when making sync calls.
      remote_.internal_state()->force_outgoing_messages_async(true);
    }

    explicit RemoteWrapper(scoped_refptr<base::SequencedTaskRunner> task_runner)
        : task_runner_(std::move(task_runner)) {}

    void BindOnTaskRunner(PendingType remote) {
      // TODO(https://crbug.com/682334): At the moment we don't have a group
      // controller available. That means the user won't be able to pass
      // associated endpoints on this interface (at least not immediately). In
      // order to fix this, we need to create a MultiplexRouter immediately and
      // bind it to the interface pointer on the |task_runner_|. Therefore,
      // MultiplexRouter should be able to be created on a sequence different
      // than the one that it is supposed to listen on.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&RemoteWrapper::Bind, this, std::move(remote)));
    }

    std::unique_ptr<ThreadSafeForwarder<InterfaceType>> CreateForwarder() {
      return std::make_unique<ThreadSafeForwarder<InterfaceType>>(
          task_runner_, base::BindRepeating(&RemoteWrapper::Accept, this),
          base::BindRepeating(&RemoteWrapper::AcceptWithResponder, this),
          base::BindRepeating(&RemoteWrapper::ForceAsyncSend, this),
          associated_group_);
    }

    void set_disconnect_handler(
        base::OnceClosure handler,
        scoped_refptr<base::SequencedTaskRunner> handler_task_runner) {
      if (!task_runner_->RunsTasksInCurrentSequence()) {
        // Make sure we modify the remote's disconnect handler on the
        // correct sequence.
        task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(&RemoteWrapper::set_disconnect_handler, this,
                           std::move(handler), std::move(handler_task_runner)));
        return;
      }
      // The actual handler will post a task to run |handler| on
      // |handler_task_runner|.
      auto wrapped_handler =
          base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                         handler_task_runner, FROM_HERE, std::move(handler));
      // Because we may have had to post a task to set this handler,
      // this call may land after the remote has just been disconnected.
      // Manually invoke the handler in that case.
      if (!remote_.is_connected()) {
        std::move(wrapped_handler).Run();
        return;
      }
      remote_.set_disconnect_handler(std::move(wrapped_handler));
    }

   private:
    friend struct RemoteWrapperDeleter;

    ~RemoteWrapper() = default;

    void Bind(PendingType remote) {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      remote_.Bind(std::move(remote));

      // By default we force all messages to behave as if async within the
      // Remote, as SharedRemote implements its own waiting mechanism to block
      // only the calling thread when making sync calls.
      remote_.internal_state()->force_outgoing_messages_async(true);
    }

    void Accept(Message message) {
      remote_.internal_state()->ForwardMessage(std::move(message));
    }

    void AcceptWithResponder(Message message,
                             std::unique_ptr<MessageReceiver> responder) {
      remote_.internal_state()->ForwardMessageWithResponder(
          std::move(message), std::move(responder));
    }

    void ForceAsyncSend(bool force) {
      remote_.internal_state()->force_outgoing_messages_async(force);
    }

    void DeleteOnCorrectThread() const {
      if (!task_runner_->RunsTasksInCurrentSequence()) {
        // NOTE: This is only called when there are no more references to
        // |this|, so binding it unretained is both safe and necessary.
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&RemoteWrapper::DeleteOnCorrectThread,
                                      base::Unretained(this)));
      } else {
        delete this;
      }
    }

    RemoteType remote_;
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;
    AssociatedGroup associated_group_;

    DISALLOW_COPY_AND_ASSIGN(RemoteWrapper);
  };

  struct RemoteWrapperDeleter {
    static void Destruct(const RemoteWrapper* wrapper) {
      wrapper->DeleteOnCorrectThread();
    }
  };

  explicit SharedRemoteBase(scoped_refptr<RemoteWrapper> wrapper)
      : wrapper_(std::move(wrapper)), forwarder_(wrapper_->CreateForwarder()) {}

  // Creates a SharedRemoteBase wrapping an underlying non-thread-safe
  // PendingType which is bound to the calling sequence. All messages sent
  // via this thread-safe proxy will internally be sent by first posting to this
  // (the calling) sequence's TaskRunner.
  static scoped_refptr<SharedRemoteBase> Create(PendingType pending_remote) {
    scoped_refptr<RemoteWrapper> wrapper =
        new RemoteWrapper(RemoteType(std::move(pending_remote)));
    return new SharedRemoteBase(wrapper);
  }

  // Creates a SharedRemoteBase which binds the underlying
  // non-thread-safe InterfacePtrType on the specified TaskRunner. All messages
  // sent via this thread-safe proxy will internally be sent by first posting to
  // that TaskRunner.
  static scoped_refptr<SharedRemoteBase> Create(
      PendingType pending_remote,
      scoped_refptr<base::SequencedTaskRunner> bind_task_runner) {
    scoped_refptr<RemoteWrapper> wrapper =
        new RemoteWrapper(std::move(bind_task_runner));
    wrapper->BindOnTaskRunner(std::move(pending_remote));
    return new SharedRemoteBase(wrapper);
  }

  ~SharedRemoteBase() = default;

  const scoped_refptr<RemoteWrapper> wrapper_;
  const std::unique_ptr<ThreadSafeForwarder<InterfaceType>> forwarder_;

  DISALLOW_COPY_AND_ASSIGN(SharedRemoteBase);
};

// SharedRemote wraps a non-thread-safe Remote and proxies messages to it.
// Unlike normal Remote objects, SharedRemote is copyable and usable from any
// thread, but has some additional overhead and latency in message transmission
// as a trade-off.
//
// Async calls are posted to the bound sequence (the sequence that the
// underlying Remote is bound to, i.e. |bind_task_runner| below), and responses
// are posted back to the calling sequence. Sync calls are dispatched directly
// if the call is made on the bound sequence, or posted otherwise.
//
// This means that in general, when making calls from sequences other than the
// bound sequence, a hop is first made *to* the bound sequence; and when
// receiving replies, a hop is made *from* the bound the sequence.
//
// Note that sync calls only block the calling sequence.
template <typename Interface>
class SharedRemote {
 public:
  // Constructs an unbound SharedRemote. This object cannot issue Interface
  // method calls and does not schedule any tasks. A default-constructed
  // SharedRemote may be replaced with a bound one via copy- or move-assignment.
  SharedRemote() = default;

  // Constructs a SharedRemote bound to `pending_remote` on the calling
  // sequence. See `Bind()` below for more details.
  explicit SharedRemote(PendingRemote<Interface> pending_remote) {
    Bind(std::move(pending_remote), nullptr);
  }

  // Constructs a SharedRemote bound to `pending_remote` on the sequence given
  // by `bind_task_runner`. See `Bind()` below for more details.
  SharedRemote(PendingRemote<Interface> pending_remote,
               scoped_refptr<base::SequencedTaskRunner> bind_task_runner) {
    Bind(std::move(pending_remote), std::move(bind_task_runner));
  }

  // SharedRemote supports both copy and move construction and assignment. These
  // are explicitly defaulted here for clarity.
  SharedRemote(const SharedRemote&) = default;
  SharedRemote(SharedRemote&&) = default;
  SharedRemote& operator=(const SharedRemote&) = default;
  SharedRemote& operator=(SharedRemote&&) = default;

  bool is_bound() const { return remote_ != nullptr; }
  explicit operator bool() const { return is_bound(); }

  Interface* get() const { return remote_->get(); }
  Interface* operator->() const { return get(); }
  Interface& operator*() const { return *get(); }

  void set_disconnect_handler(
      base::OnceClosure handler,
      scoped_refptr<base::SequencedTaskRunner> handler_task_runner) {
    remote_->set_disconnect_handler(std::move(handler),
                                    std::move(handler_task_runner));
  }

  // Clears this SharedRemote. Note that this does *not* necessarily close the
  // remote's endpoint as other SharedRemote instances may reference the same
  // underlying endpoint.
  void reset() { remote_.reset(); }

  // Binds this SharedRemote to `pending_remote` on the sequence given by
  // `bind_task_runner`, or the calling sequence if `bind_task_runner` is null.
  // Once bound, the SharedRemote may be used to send messages on the underlying
  // Remote. Messages always bounce through `bind_task_runner` before sending,
  // unless the caller is issuing a [Sync] call from `bind_task_runner` already;
  // in which case this behaves exactly like a regular Remote for that call.
  //
  // Any reply received by the SharedRemote is dispatched to whatever
  // SequencedTaskRunner was current when the corresponding request was made.
  //
  // A bound SharedRemote may be copied any number of times, to any number of
  // threads. Each copy sends messages through the same underlying Remote, after
  // bouncing through the same `bind_task_runner`.
  //
  // If this SharedRemote was already bound, it will be effectively unbound by
  // this call and re-bound to `pending_remote`. Any prior copies made are NOT
  // affected and will retain their reference to the original Remote.
  void Bind(PendingRemote<Interface> pending_remote,
            scoped_refptr<base::SequencedTaskRunner> bind_task_runner) {
    if (bind_task_runner && pending_remote) {
      remote_ = SharedRemoteBase<Remote<Interface>>::Create(
          std::move(pending_remote), std::move(bind_task_runner));
    } else if (pending_remote) {
      remote_ = SharedRemoteBase<Remote<Interface>>::Create(
          std::move(pending_remote));
    }
  }

 private:
  scoped_refptr<SharedRemoteBase<Remote<Interface>>> remote_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SHARED_REMOTE_H_
