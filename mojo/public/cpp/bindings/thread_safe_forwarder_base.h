// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_FORWARDER_BASE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_FORWARDER_BASE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) ThreadSafeForwarderBase
    : public MessageReceiverWithResponder {
 public:
  using ForwardMessageCallback = base::RepeatingCallback<void(Message)>;
  using ForwardMessageWithResponderCallback =
      base::RepeatingCallback<void(Message, std::unique_ptr<MessageReceiver>)>;

  ThreadSafeForwarderBase(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      ForwardMessageCallback forward,
      ForwardMessageWithResponderCallback forward_with_responder,
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
  AssociatedGroup associated_group_;
  scoped_refptr<InProgressSyncCalls> sync_calls_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSafeForwarderBase);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_THREAD_SAFE_FORWARDER_BASE_H_
