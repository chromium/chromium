// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CLIENT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/dcheck_is_on.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/lib/control_message_handler.h"
#include "mojo/public/cpp/bindings/lib/control_message_proxy.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_dispatcher.h"
#include "mojo/public/cpp/bindings/message_metadata_helpers.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/bindings/thread_safe_proxy.h"

namespace mojo {

class AssociatedGroup;
class InterfaceEndpointController;

// InterfaceEndpointClient handles message sending and receiving of an interface
// endpoint, either the implementation side or the client side.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) InterfaceEndpointClient
    : public MessageReceiverWithResponder {
 public:
  // Constructs a new InterfaceEndpointClient for use on `task_runner`. Unless
  // otherwise noted, all methods (including the destructor) must be called on
  // `task_runner`. This does not need to run tasks on the same sequence that
  // called the constructor.
  //
  // `receiver` may be null, but if non-null it must outlive this object.
  InterfaceEndpointClient(ScopedInterfaceEndpointHandle handle,
                          MessageReceiverWithResponderStatus* receiver,
                          std::unique_ptr<MessageReceiver> payload_validator,
                          base::span<const uint32_t> sync_method_ordinals,
                          scoped_refptr<base::SequencedTaskRunner> task_runner,
                          uint32_t interface_version,
                          const char* interface_name,
                          MessageToMethodInfoCallback method_info_callback,
                          MessageToMethodNameCallback method_name_callback);

  InterfaceEndpointClient(const InterfaceEndpointClient&) = delete;
  InterfaceEndpointClient& operator=(const InterfaceEndpointClient&) = delete;

  ~InterfaceEndpointClient() override;

  // Sets the error handler to receive notifications when an error is
  // encountered.
  void set_connection_error_handler(base::OnceClosure error_handler) {
    CHECK(sequence_checker_.CalledOnValidSequence());
    error_handler_ = std::move(error_handler);
    error_with_reason_handler_.Reset();
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    CHECK(sequence_checker_.CalledOnValidSequence());
    error_with_reason_handler_ = std::move(error_handler);
    error_handler_.Reset();
  }

  // Returns true if an error was encountered.
  bool encountered_error() const {
    CHECK(sequence_checker_.CalledOnValidSequence());
    return encountered_error_;
  }

  // Returns true if this endpoint has any pending callbacks.
  bool has_pending_responders() const {
    CHECK(sequence_checker_.CalledOnValidSequence());
    base::AutoLock lock(async_responders_lock_);
    return !async_responders_.empty() || !sync_responses_.empty();
  }

  AssociatedGroup* associated_group();

  scoped_refptr<ThreadSafeProxy> CreateThreadSafeProxy(
      scoped_refptr<ThreadSafeProxy::Target> target);

  // Sets a MessageFilter which can filter a message after validation but
  // before dispatch.
  void SetFilter(std::unique_ptr<MessageFilter> filter);

  // After this call the object is in an invalid state and shouldn't be reused.
  ScopedInterfaceEndpointHandle PassHandle();

  // Raises an error on the underlying message pipe. It disconnects the pipe
  // and notifies all interfaces running on this pipe.
  void RaiseError();

  void CloseWithReason(uint32_t custom_reason, std::string_view description);

  // Used by ControlMessageProxy to send messages through this endpoint.
  void SendControlMessage(Message* message);
  void SendControlMessageWithResponder(
      Message* message,
      std::unique_ptr<MessageReceiver> responder);

  // MessageReceiverWithResponder implementation:
  // They must only be called when the handle is not in pending association
  // state.
  bool PrefersSerializedMessages() override;
  bool Accept(Message* message) override;
  bool AcceptWithResponder(Message* message,
                           std::unique_ptr<MessageReceiver> responder) override;

  // Controls how sync messages are forwarded.
  enum class SyncSendMode {
    // Allows the InterfaceEndpointClient to do its own internal sync wait when
    // sending a sync message. Used in the common case where the reply is waited
    // upon from the InterfaceEndpointClient's bound sequence.
    kAllowSyncWait,

    // Forces the InterfaceEndpointClient to send a sync message as if it were
    // async, leaving any waiting up to the caller.
    kForceAsync,
  };

  // Implementations used by both SendControlMessage* and Accept* above.
  bool SendMessage(Message* message, bool is_control_message);
  bool SendMessageWithResponder(Message* message,
                                bool is_control_message,
                                SyncSendMode sync_send_mode,
                                std::unique_ptr<MessageReceiver> responder);

  // The following methods are called by the router. They must be called
  // outside of the router's lock.

  // NOTE: |message| must have passed message header validation.
  bool HandleIncomingMessage(Message* message);
  void NotifyError(const std::optional<DisconnectReason>& reason);

  // The following methods send interface control messages.
  // They must only be called when the handle is not in pending association
  // state.
  void QueryVersion(base::OnceCallback<void(uint32_t)> callback);
  void RequireVersion(uint32_t version);
  void FlushForTesting();
  void FlushAsyncForTesting(base::OnceClosure callback);

  // Sets a callback to handle idle notifications. This callback will be invoked
  // any time the peer endpoint sends a NotifyIdle control message AND
  // |num_unacked_messages_| is zero.
  //
  // Configures the peer endpoint to ack incoming messages send NotifyIdle
  // notifications only once it's been idle continuously for at least a duration
  // of |timeout|.
  void SetIdleHandler(base::TimeDelta timeout, base::RepeatingClosure handler);

  unsigned int GetNumUnackedMessagesForTesting() const {
    return num_unacked_messages_;
  }

  // Sets a callback to invoke whenever this endpoint receives an
  // EnableIdleTracking message from its peer. The callback is invoked with a
  // new ConnectionGroup Ref that is expected to be adopted by whatever owns
  // this endpoint.
  using IdleTrackingEnabledCallback =
      base::OnceCallback<void(ConnectionGroup::Ref connection_group)>;
  void SetIdleTrackingEnabledCallback(IdleTrackingEnabledCallback callback);

  // Called by the ControlMessageHandler when receiving corresponding control
  // messages.
  bool AcceptEnableIdleTracking(base::TimeDelta timeout);
  bool AcceptMessageAck();
  bool AcceptNotifyIdle();

  void MaybeStartIdleTimer();
  void MaybeSendNotifyIdle();

  const char* interface_name() const { return interface_name_; }
  MessageToMethodInfoCallback method_info_callback() const {
    return method_info_callback_;
  }
  MessageToMethodNameCallback method_name_callback() const {
    return method_name_callback_;
  }

#if DCHECK_IS_ON()
  void SetNextCallLocation(const base::Location& location) {
    next_call_location_ = location;
  }
#endif

  // This allows the endpoint to be reset from a sequence other than the one on
  // which it was bound. This should only be used with caution, and it is
  // critical that the calling sequence cannot run tasks concurrently with the
  // bound sequence. There's no practical way for this to be asserted, so we
  // have to take your word for it. If this constraint is not upheld, there will
  // be data races internal to the bindings object which can lead to UAFs or
  // surprise message dispatches.
  void ResetFromAnotherSequenceUnsafe();

  // Tells the client to forget about a pending async request for which it still
  // hasn't seen a response. Called by the router, possibly from other threads.
  // The router lock must be held when calling this.
  void ForgetAsyncRequest(uint64_t request_id);

  base::span<const uint32_t> sync_method_ordinals() const {
    return sync_method_ordinals_;
  }

  // If adding a new call to this function, be sure to update the list of
  // suffixes in histograms.xml
  static void SetThreadNameSuffixForMetrics(std::string thread_name);

 private:
  struct PendingAsyncResponse {
   public:
    PendingAsyncResponse(uint32_t request_message_name,
                         std::unique_ptr<MessageReceiver> responder);
    PendingAsyncResponse(PendingAsyncResponse&&);
    PendingAsyncResponse(const PendingAsyncResponse&) = delete;
    PendingAsyncResponse& operator=(PendingAsyncResponse&&);
    PendingAsyncResponse& operator=(const PendingAsyncResponse&) = delete;
    ~PendingAsyncResponse();

    uint32_t request_message_name;
    std::unique_ptr<MessageReceiver> responder;
  };

  using AsyncResponderMap = std::map<uint64_t, PendingAsyncResponse>;

  struct SyncResponseInfo {
   public:
    SyncResponseInfo(uint32_t request_message_name, bool* in_response_received);

    SyncResponseInfo(const SyncResponseInfo&) = delete;
    SyncResponseInfo& operator=(const SyncResponseInfo&) = delete;

    ~SyncResponseInfo();

    uint32_t request_message_name;
    Message response;

    // Points to a stack-allocated variable.
    raw_ptr<bool> response_received;
  };

  using SyncResponseMap = std::map<uint64_t, std::unique_ptr<SyncResponseInfo>>;

  // Used as the sink for |payload_validator_| and forwards messages to
  // HandleValidatedMessage().
  class HandleIncomingMessageThunk : public MessageReceiver {
   public:
    explicit HandleIncomingMessageThunk(InterfaceEndpointClient* owner);

    HandleIncomingMessageThunk(const HandleIncomingMessageThunk&) = delete;
    HandleIncomingMessageThunk& operator=(const HandleIncomingMessageThunk&) =
        delete;

    ~HandleIncomingMessageThunk() override;

    // MessageReceiver implementation:
    bool Accept(Message* message) override;

   private:
    // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of
    // speedometer3).
    RAW_PTR_EXCLUSION InterfaceEndpointClient* const owner_ = nullptr;
  };

  void InitControllerIfNecessary();

  void OnAssociationEvent(
      ScopedInterfaceEndpointHandle::AssociationEvent event);

  bool HandleValidatedMessage(Message* message);

  const base::raw_span<const uint32_t> sync_method_ordinals_;

  // The callback to invoke when our peer endpoint sends us NotifyIdle and we
  // have no outstanding unacked messages. If null, no callback has been set and
  // we do not expect to receive NotifyIdle or MessageAck messages from the
  // peer.
  base::RepeatingClosure idle_handler_;

  // A callback to invoke if and when this endpoint receives an
  // EnableIdleTracking control message.
  IdleTrackingEnabledCallback idle_tracking_enabled_callback_;

  // The timeout to wait for continuous idling before notiftying our peer that
  // we're idle.
  std::optional<base::TimeDelta> idle_timeout_;

  // The current idle timer, valid only while we're idle. If this fires, we send
  // a NotifyIdle to our peer.
  std::optional<base::OneShotTimer> notify_idle_timer_;

  // A ref to a ConnectionGroup used to track the idle state of this endpoint,
  // if any. Only non-null if an EnableIdleTracking message has been received.
  // This is a weak ref to the group.
  ConnectionGroup::Ref idle_tracking_connection_group_;

  // Indicates the number of unacked messages that have been sent so far. Only
  // non-zero when |idle_handler_| has been set and some number of unacked
  // messages remain in-flight.
  unsigned int num_unacked_messages_ = 0;

  ScopedInterfaceEndpointHandle handle_;
  std::unique_ptr<AssociatedGroup> associated_group_;
  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of sampling
  // profiler data).
  RAW_PTR_EXCLUSION InterfaceEndpointController* controller_ = nullptr;

  // RAW_PTR_EXCLUSION: Performance reasons (based on analysis of sampling
  // profiler data).
  RAW_PTR_EXCLUSION MessageReceiverWithResponderStatus* const
      incoming_receiver_ = nullptr;
  HandleIncomingMessageThunk thunk_{this};
  MessageDispatcher dispatcher_;

  mutable base::Lock async_responders_lock_;
  AsyncResponderMap async_responders_ GUARDED_BY(async_responders_lock_);
  SyncResponseMap sync_responses_;

  uint64_t next_request_id_ = 1;

  base::OnceClosure error_handler_;
  ConnectionErrorWithReasonCallback error_with_reason_handler_;
  bool encountered_error_ = false;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  internal::ControlMessageProxy control_message_proxy_{this};
  internal::ControlMessageHandler control_message_handler_;
  const char* const interface_name_;
  const MessageToMethodInfoCallback method_info_callback_;
  const MessageToMethodNameCallback method_name_callback_;

#if DCHECK_IS_ON()
  // The code location of the the most recent call into a method on this
  // interface endpoint. This is set *after* the call but *before* any message
  // is actually transmitted for it.
  base::Location next_call_location_;
#endif

  // We use SequenceCheckerImpl directly, to assert some sequence checks even in
  // release builds. See https://crbug.com/1325096.
  base::SequenceCheckerImpl sequence_checker_;

  base::WeakPtrFactory<InterfaceEndpointClient> weak_ptr_factory_{this};
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CLIENT_H_
