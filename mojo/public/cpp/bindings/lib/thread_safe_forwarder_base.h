// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_SAFE_FORWARDER_BASE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_SAFE_FORWARDER_BASE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

namespace internal {

// This class defines out-of-line logic common to the behavior of
// ThreadSafeForwarder<Interface>, which is in turn used to support the
// implementation of SharedRemote<Interface>.
//
// This object is sequence-affine and it provides an opaque interface to an
// underlying weakly-referenced interface proxy (e.g. a Remote) which may be
// bound on a different sequence and referenced weakly by any number of other
// ThreadSafeForwarders. The opaque interface is provide via a set of callbacks
// bound internally by e.g. SharedRemote.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ThreadSafeForwarderBase
    : public MessageReceiverWithResponder {
 public:
  // A callback used to send a message on the underlying interface proxy. Used
  // only for messages with no reply.
  using ForwardMessageCallback = base::RepeatingCallback<void(Message)>;

  // A callback used to send a message on the underlying interface proxy. Used
  // only for messages with no reply.
  using ForwardMessageWithResponderCallback =
      base::RepeatingCallback<void(Message, std::unique_ptr<MessageReceiver>)>;

  // A callback used to reconfigure the underlying proxy by changing whether or
  // not it can perform its own blocking waits for [Sync] message replies. When
  // `force` is false, the proxy behaves normally and will block the calling
  // thread when used to issue a sync message; when `force` is true however,
  // [Sync] is effectively ignored when sending messages and the reply is
  // received asynchronously. ThreadSafeForwarderBase uses this to disable
  // normal synchronous behavior and implement its own sync waiting from the
  // caller's thread rather than the proxy's bound thread.
  using ForceAsyncSendCallback = base::RepeatingCallback<void(bool force)>;

  // Constructs a new ThreadSafeForwarderBase which forwards requests to a proxy
  // bound on `task_runner`. Forwarding is done opaquely via the callbacks given
  // in `forward` (to send one-off messages), `forward_with_responder` (to send
  // messages expecting replies), and `force_async_send` to control sync IPC
  // behavior within the underlying proxy.
  ThreadSafeForwarderBase(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ForwardMessageCallback forward,
      ForwardMessageWithResponderCallback forward_with_responder,
      ForceAsyncSendCallback force_async_send,
      const AssociatedGroup& associated_group);

  ~ThreadSafeForwarderBase() override;

  // MessageReceiverWithResponder implementation:
  bool PrefersSerializedMessages() override;
  bool Accept(Message* message) override;
  bool AcceptWithResponder(Message* message,
                           std::unique_ptr<MessageReceiver> responder) override;

 private:
  // Data that we need to share between the sequences involved in a sync call.
  struct SyncResponseInfo
      : public base::RefCountedThreadSafe<SyncResponseInfo> {
    SyncResponseInfo();

    Message message;
    bool received = false;
    base::WaitableEvent event{base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED};

   private:
    friend class base::RefCountedThreadSafe<SyncResponseInfo>;

    ~SyncResponseInfo();
  };

  // A MessageReceiver that signals |response| when it either accepts the
  // response message, or is destructed.
  class SyncResponseSignaler : public MessageReceiver {
   public:
    explicit SyncResponseSignaler(scoped_refptr<SyncResponseInfo> response);
    ~SyncResponseSignaler() override;

    bool Accept(Message* message) override;

   private:
    scoped_refptr<SyncResponseInfo> response_;
  };

  // A record of the pending sync responses for canceling pending sync calls
  // when the owning ThreadSafeForwarder is destructed.
  struct InProgressSyncCalls
      : public base::RefCountedThreadSafe<InProgressSyncCalls> {
    InProgressSyncCalls();

    // |lock| protects access to |pending_responses|.
    base::Lock lock;
    std::vector<SyncResponseInfo*> pending_responses;

   private:
    friend class base::RefCountedThreadSafe<InProgressSyncCalls>;

    ~InProgressSyncCalls();
  };

  class ForwardToCallingThread : public MessageReceiver {
   public:
    explicit ForwardToCallingThread(std::unique_ptr<MessageReceiver> responder);
    ~ForwardToCallingThread() override;

   private:
    bool Accept(Message* message) override;

    static void CallAcceptAndDeleteResponder(
        std::unique_ptr<MessageReceiver> responder,
        Message message);

    std::unique_ptr<MessageReceiver> responder_;
    scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;
  };

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const ForwardMessageCallback forward_;
  const ForwardMessageWithResponderCallback forward_with_responder_;
  const ForceAsyncSendCallback force_async_send_;
  AssociatedGroup associated_group_;
  scoped_refptr<InProgressSyncCalls> sync_calls_;
  int sync_call_nesting_level_ = 0;
  base::WeakPtrFactory<ThreadSafeForwarderBase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeForwarderBase);
};

}  // namespace internal

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_THREAD_SAFE_FORWARDER_BASE_H_
