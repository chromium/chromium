// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_SHARED_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_SHARED_REMOTE_H_

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/task/task_runner.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/lib/thread_safe_forwarder_base.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/runtime_features.h"

namespace mojo {

namespace internal {

template <typename RemoteType>
struct SharedRemoteTraits;

template <typename Interface>
struct SharedRemoteTraits<Remote<Interface>> {
  static void BindDisconnected(Remote<Interface>& remote) {
    std::ignore = remote.BindNewPipeAndPassReceiver();
  }
};

}  // namespace internal

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
  explicit ThreadSafeForwarder(scoped_refptr<ThreadSafeProxy> thread_safe_proxy)
      : ThreadSafeForwarderBase(std::move(thread_safe_proxy)), proxy_(this) {}

  ThreadSafeForwarder(const ThreadSafeForwarder&) = delete;
  ThreadSafeForwarder& operator=(const ThreadSafeForwarder&) = delete;

  ~ThreadSafeForwarder() override = default;

  ProxyType& proxy() { return proxy_; }

 private:
  ProxyType proxy_;
};

template <typename Interface>
class SharedRemote;

template <typename RemoteType>
class SharedRemoteBase
    : public base::RefCountedThreadSafe<SharedRemoteBase<RemoteType>> {
 public:
  using InterfaceType = typename RemoteType::InterfaceType;
  using PendingType = typename RemoteType::PendingType;

  SharedRemoteBase(const SharedRemoteBase&) = delete;
  SharedRemoteBase& operator=(const SharedRemoteBase&) = delete;

  InterfaceType* get() { return &forwarder_->proxy(); }
  InterfaceType* operator->() { return get(); }
  InterfaceType& operator*() { return *get(); }

  void set_disconnect_handler(
      base::OnceClosure handler,
      scoped_refptr<base::SequencedTaskRunner> handler_task_runner) {
    wrapper_->set_disconnect_handler(std::move(handler),
                                     std::move(handler_task_runner));
  }

  void Disconnect() { wrapper_->Disconnect(); }

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
    RemoteWrapper(PendingType remote,
                  scoped_refptr<base::SequencedTaskRunner> task_runner)
        : task_runner_(std::move(task_runner)),
          remote_(std::move(remote), task_runner_),
          associated_group_(*remote_.internal_state()->associated_group()) {}

    RemoteWrapper(const RemoteWrapper&) = delete;
    RemoteWrapper& operator=(const RemoteWrapper&) = delete;

    std::unique_ptr<ThreadSafeForwarder<InterfaceType>> CreateForwarder() {
      return std::make_unique<ThreadSafeForwarder<InterfaceType>>(
          remote_.internal_state()->CreateThreadSafeProxy(
              base::MakeRefCounted<ProxyTarget>(this)));
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

    void Disconnect() {
      if (!task_runner_->RunsTasksInCurrentSequence()) {
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&RemoteWrapper::Disconnect, this));
        return;
      }

      // Reset the remote and rebind it in a disconnected state, so that it's
      // usable but discards all messages.
      remote_.reset();
      internal::SharedRemoteTraits<RemoteType>::BindDisconnected(remote_);
    }

   private:
    friend struct RemoteWrapperDeleter;
    friend class base::DeleteHelper<RemoteWrapper>;

    ~RemoteWrapper() = default;

    // This provides a roundabout way for a ThreadSafeProxy to hold a reference
    // back to the RemoteWrapper which created it. The purpose is to ensure that
    // the RemoteWrapper lives at least as long as the ThreadSafeProxy, which in
    // turn ensures that it lives at least as long as any outgoing message task.
    class ProxyTarget : public ThreadSafeProxy::Target {
     public:
      explicit ProxyTarget(scoped_refptr<RemoteWrapper> wrapper)
          : wrapper_(std::move(wrapper)) {}

     private:
      ~ProxyTarget() override = default;

      const scoped_refptr<RemoteWrapper> wrapper_;
    };

    void DeleteOnCorrectThread() const {
      if (!task_runner_->RunsTasksInCurrentSequence()) {
        // NOTE: This is only called when there are no more references to
        // |this|, so binding it unretained is both safe and necessary.
        task_runner_->DeleteSoon(FROM_HERE, this);
      } else {
        delete this;
      }
    }

    const scoped_refptr<base::SequencedTaskRunner> task_runner_;
    RemoteType remote_;
    AssociatedGroup associated_group_;
  };

  struct RemoteWrapperDeleter {
    static void Destruct(const RemoteWrapper* wrapper) {
      wrapper->DeleteOnCorrectThread();
    }
  };

  explicit SharedRemoteBase(scoped_refptr<RemoteWrapper> wrapper)
      : wrapper_(std::move(wrapper)), forwarder_(wrapper_->CreateForwarder()) {}

  // Creates a SharedRemoteBase bound to `pending_remote`. All messages sent
  // through the SharedRemote will first bounce through `task_runner`.
  static scoped_refptr<SharedRemoteBase> Create(
      PendingType pending_remote,
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return new SharedRemoteBase(base::MakeRefCounted<RemoteWrapper>(
        std::move(pending_remote), std::move(task_runner)));
  }

  ~SharedRemoteBase() = default;

  const scoped_refptr<RemoteWrapper> wrapper_;
  const std::unique_ptr<ThreadSafeForwarder<InterfaceType>> forwarder_;
};

// SharedRemote wraps a non-thread-safe Remote and proxies messages to it. Note
// that SharedRemote itself is also NOT THREAD-SAFE, but unlike Remote it IS
// copyable cross-thread, and each copy is usable from its own thread. The
// trade-off compared to a Remote is some additional overhead and latency in
// message transmission, as sending a message usually incurs a task hop.
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

  // Disconnects the SharedRemote. This leaves the object in a usable state --
  // i.e. it's still safe to dereference and make calls -- but severs the
  // underlying connection so that no new replies will be received and all
  // outgoing messages will be discarded. This is useful when you want to force
  // a disconnection like with reset(), but you don't want the SharedRemote to
  // become unbound.
  void Disconnect() { remote_->Disconnect(); }

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
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      remote_.reset();
      return;
    }
    if (bind_task_runner && pending_remote) {
      remote_ = SharedRemoteBase<Remote<Interface>>::Create(
          std::move(pending_remote), std::move(bind_task_runner));
    } else if (pending_remote) {
      remote_ = SharedRemoteBase<Remote<Interface>>::Create(
          std::move(pending_remote),
          base::SequencedTaskRunner::GetCurrentDefault());
    }
  }

  // Creates a new pipe, binding this SharedRemote to one end on
  // `bind_task_runner` and returning the other end as a PendingReceiver.
  PendingReceiver<Interface> BindNewPipeAndPassReceiver(
      scoped_refptr<base::SequencedTaskRunner> bind_task_runner = nullptr) {
    if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
      return PendingReceiver<Interface>();
    }
    PendingRemote<Interface> remote;
    auto receiver = remote.InitWithNewPipeAndPassReceiver();
    Bind(std::move(remote), std::move(bind_task_runner));
    return receiver;
  }

 private:
  scoped_refptr<SharedRemoteBase<Remote<Interface>>> remote_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_SHARED_REMOTE_H_
