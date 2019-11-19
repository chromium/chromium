// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_INTERFACE_PTR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_INTERFACE_PTR_H_

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/deprecated_interface_types_forward.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/bindings/thread_safe_forwarder_base.h"

// ThreadSafeInterfacePtr wraps a non-thread-safe InterfacePtr and proxies
// messages to it. Async calls are posted to the sequence that the InteracePtr
// is bound to, and the responses are posted back. Sync calls are dispatched
// directly if the call is made on the sequence that the wrapped InterfacePtr is
// bound to, or posted otherwise. It's important to be aware that sync calls
// block both the calling sequence and the InterfacePtr sequence. That means
// that you cannot make sync calls through a ThreadSafeInterfacePtr if the
// underlying InterfacePtr is bound to a sequence that cannot block, like the IO
// thread.

namespace mojo {

// Instances of this class may be used from any sequence to serialize
// |Interface| messages and forward them elsewhere. In general you should use
// one of the ThreadSafeInterfacePtrBase helper aliases defined below, but this
// type may be useful if you need/want to manually manage the lifetime of the
// underlying proxy object which will be used to ultimately send messages.
template <typename Interface>
class ThreadSafeForwarder : public ThreadSafeForwarderBase {
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
      const AssociatedGroup& associated_group)
      : ThreadSafeForwarderBase(std::move(task_runner),
                                std::move(forward),
                                std::move(forward_with_responder),
                                associated_group),
        proxy_(this) {}

  ~ThreadSafeForwarder() override = default;

  ProxyType& proxy() { return proxy_; }

 private:
  ProxyType proxy_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeForwarder);
};

template <typename InterfacePtrType>
class ThreadSafeInterfacePtrBase
    : public base::RefCountedThreadSafe<
          ThreadSafeInterfacePtrBase<InterfacePtrType>> {
 public:
  using InterfaceType = typename InterfacePtrType::InterfaceType;
  using PtrInfoType = typename InterfacePtrType::PtrInfoType;

  explicit ThreadSafeInterfacePtrBase(
      std::unique_ptr<ThreadSafeForwarder<InterfaceType>> forwarder)
      : forwarder_(std::move(forwarder)) {}

  // Creates a ThreadSafeInterfacePtrBase wrapping an underlying non-thread-safe
  // InterfacePtrType which is bound to the calling sequence. All messages sent
  // via this thread-safe proxy will internally be sent by first posting to this
  // (the calling) sequence's TaskRunner.
  static scoped_refptr<ThreadSafeInterfacePtrBase> Create(
      InterfacePtrType interface_ptr) {
    scoped_refptr<PtrWrapper> wrapper =
        new PtrWrapper(std::move(interface_ptr));
    return new ThreadSafeInterfacePtrBase(wrapper->CreateForwarder());
  }

  // Creates a ThreadSafeInterfacePtrBase which binds the underlying
  // non-thread-safe InterfacePtrType on the specified TaskRunner. All messages
  // sent via this thread-safe proxy will internally be sent by first posting to
  // that TaskRunner.
  static scoped_refptr<ThreadSafeInterfacePtrBase> Create(
      PtrInfoType ptr_info,
      const scoped_refptr<base::SequencedTaskRunner>& bind_task_runner) {
    scoped_refptr<PtrWrapper> wrapper = new PtrWrapper(bind_task_runner);
    wrapper->BindOnTaskRunner(std::move(ptr_info));
    return new ThreadSafeInterfacePtrBase(wrapper->CreateForwarder());
  }

  InterfaceType* get() { return &forwarder_->proxy(); }
  InterfaceType* operator->() { return get(); }
  InterfaceType& operator*() { return *get(); }

 private:
  friend class base::RefCountedThreadSafe<
      ThreadSafeInterfacePtrBase<InterfacePtrType>>;

  struct PtrWrapperDeleter;

  // Helper class which owns an |InterfacePtrType| instance on an appropriate
  // sequence. This is kept alive as long its bound within some
  // ThreadSafeForwarder's callbacks.
  class PtrWrapper
      : public base::RefCountedThreadSafe<PtrWrapper, PtrWrapperDeleter> {
   public:
    explicit PtrWrapper(InterfacePtrType ptr)
        : PtrWrapper(base::SequencedTaskRunnerHandle::Get()) {
      ptr_ = std::move(ptr);
      associated_group_ = *ptr_.internal_state()->associated_group();
    }

    explicit PtrWrapper(
        const scoped_refptr<base::SequencedTaskRunner>& task_runner)
        : task_runner_(task_runner) {}

    void BindOnTaskRunner(AssociatedInterfacePtrInfo<InterfaceType> ptr_info) {
      associated_group_ = AssociatedGroup(ptr_info.handle());
      task_runner_->PostTask(FROM_HERE, base::BindOnce(&PtrWrapper::Bind, this,
                                                       std::move(ptr_info)));
    }

    void BindOnTaskRunner(InterfacePtrInfo<InterfaceType> ptr_info) {
      // TODO(yzhsen): At the momment we don't have a group controller
      // available. That means the user won't be able to pass associated
      // endpoints on this interface (at least not immediately). In order to fix
      // this, we need to create a MultiplexRouter immediately and bind it to
      // the interface pointer on the |task_runner_|. Therefore, MultiplexRouter
      // should be able to be created on a sequence different than the one that
      // it is supposed to listen on. crbug.com/682334
      task_runner_->PostTask(FROM_HERE, base::BindOnce(&PtrWrapper::Bind, this,
                                                       std::move(ptr_info)));
    }

    std::unique_ptr<ThreadSafeForwarder<InterfaceType>> CreateForwarder() {
      return std::make_unique<ThreadSafeForwarder<InterfaceType>>(
          task_runner_, base::BindRepeating(&PtrWrapper::Accept, this),
          base::BindRepeating(&PtrWrapper::AcceptWithResponder, this),
          associated_group_);
    }

   private:
    friend struct PtrWrapperDeleter;

    ~PtrWrapper() {}

    void Bind(PtrInfoType ptr_info) {
      DCHECK(task_runner_->RunsTasksInCurrentSequence());
      ptr_.Bind(std::move(ptr_info));
    }

    void Accept(Message message) {
      ptr_.internal_state()->ForwardMessage(std::move(message));
    }

    void AcceptWithResponder(Message message,
                             std::unique_ptr<MessageReceiver> responder) {
      ptr_.internal_state()->ForwardMessageWithResponder(std::move(message),
                                                         std::move(responder));
    }

    void DeleteOnCorrectThread() const {
      if (!task_runner_->RunsTasksInCurrentSequence()) {
        // NOTE: This is only called when there are no more references to
        // |this|, so binding it unretained is both safe and necessary.
        task_runner_->PostTask(
            FROM_HERE, base::BindOnce(&PtrWrapper::DeleteOnCorrectThread,
                                      base::Unretained(this)));
      } else {
        delete this;
      }
    }

    InterfacePtrType ptr_;
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;
    AssociatedGroup associated_group_;

    DISALLOW_COPY_AND_ASSIGN(PtrWrapper);
  };

  struct PtrWrapperDeleter {
    static void Destruct(const PtrWrapper* interface_ptr) {
      interface_ptr->DeleteOnCorrectThread();
    }
  };

  ~ThreadSafeInterfacePtrBase() {}

  const std::unique_ptr<ThreadSafeForwarder<InterfaceType>> forwarder_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeInterfacePtrBase);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_INTERFACE_PTR_H_
