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
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/bindings/sync_event_watcher.h"
#include "mojo/public/cpp/bindings/thread_safe_interface_ptr.h"

namespace mojo {

template <typename RemoteType>
class SharedRemoteBase
    : public base::RefCountedThreadSafe<SharedRemoteBase<RemoteType>> {
 public:
  using InterfaceType = typename RemoteType::InterfaceType;
  using PendingType = typename RemoteType::PendingType;

  explicit SharedRemoteBase(
      std::unique_ptr<ThreadSafeForwarder<InterfaceType>> forwarder)
      : forwarder_(std::move(forwarder)) {}

  // Creates a SharedRemoteBase wrapping an underlying non-thread-safe
  // PendingType which is bound to the calling sequence. All messages sent
  // via this thread-safe proxy will internally be sent by first posting to this
  // (the calling) sequence's TaskRunner.
  static scoped_refptr<SharedRemoteBase> Create(PendingType pending_remote) {
    scoped_refptr<RemoteWrapper> wrapper =
        new RemoteWrapper(RemoteType(std::move(pending_remote)));
    return new SharedRemoteBase(wrapper->CreateForwarder());
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
    return new SharedRemoteBase(wrapper->CreateForwarder());
  }

  InterfaceType* get() { return &forwarder_->proxy(); }
  InterfaceType* operator->() { return get(); }
  InterfaceType& operator*() { return *get(); }

 private:
  friend class base::RefCountedThreadSafe<SharedRemoteBase<RemoteType>>;

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
          associated_group_);
    }

   private:
    friend struct RemoteWrapperDeleter;

    ~RemoteWrapper() {}

    void Bind(PendingType remote) {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      remote_.Bind(std::move(remote));

      // The ThreadSafeForwarder will always block the calling thread on a
      // reply, so there's no need for the endpoint to employ its own sync
      // waiting logic.
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

  ~SharedRemoteBase() {}

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
  SharedRemote() = default;
  explicit SharedRemote(PendingRemote<Interface> pending_remote)
      : remote_(pending_remote.is_valid()
                    ? SharedRemoteBase<Remote<Interface>>::Create(
                          std::move(pending_remote))
                    : nullptr) {}
  SharedRemote(PendingRemote<Interface> pending_remote,
               scoped_refptr<base::SequencedTaskRunner> bind_task_runner)
      : remote_(pending_remote.is_valid()
                    ? SharedRemoteBase<Remote<Interface>>::Create(
                          std::move(pending_remote),
                          std::move(bind_task_runner))
                    : nullptr) {}

  bool is_bound() const { return remote_ != nullptr; }
  explicit operator bool() const { return is_bound(); }

  Interface* get() const { return remote_->get(); }
  Interface* operator->() const { return get(); }
  Interface& operator*() const { return *get(); }

  // Clears this SharedRemote. Note that this does *not* necessarily close the
  // remote's endpoint as other SharedRemote instances may reference the same
  // underlying endpoint.
  void reset() { remote_.reset(); }

 private:
  scoped_refptr<SharedRemoteBase<Remote<Interface>>> remote_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SHARED_REMOTE_H_
