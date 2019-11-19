// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CLIENT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/connection_error_callback.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/lib/control_message_handler.h"
#include "mojo/public/cpp/bindings/lib/control_message_proxy.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_dispatcher.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace mojo {

class AssociatedGroup;
class InterfaceEndpointController;

// InterfaceEndpointClient handles message sending and receiving of an interface
// endpoint, either the implementation side or the client side.
// It should only be accessed and destructed on the creating sequence.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) InterfaceEndpointClient
    : public MessageReceiverWithResponder {
 public:
  // |receiver| is okay to be null. If it is not null, it must outlive this
  // object.
  InterfaceEndpointClient(ScopedInterfaceEndpointHandle handle,
                          MessageReceiverWithResponderStatus* receiver,
                          std::unique_ptr<MessageReceiver> payload_validator,
                          bool expect_sync_requests,
                          scoped_refptr<base::SequencedTaskRunner> runner,
                          uint32_t interface_version,
                          const char* interface_name);
  ~InterfaceEndpointClient() override;

  // Sets the error handler to receive notifications when an error is
  // encountered.
  void set_connection_error_handler(base::OnceClosure error_handler) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    error_handler_ = std::move(error_handler);
    error_with_reason_handler_.Reset();
  }

  void set_connection_error_with_reason_handler(
      ConnectionErrorWithReasonCallback error_handler) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    error_with_reason_handler_ = std::move(error_handler);
    error_handler_.Reset();
  }

  // Returns true if an error was encountered.
  bool encountered_error() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return encountered_error_;
  }

  // Returns true if this endpoint has any pending callbacks.
  bool has_pending_responders() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !async_responders_.empty() || !sync_responses_.empty();
  }

  AssociatedGroup* associated_group();

  // Sets a MessageFilter which can filter a message after validation but
  // before dispatch.
  void SetFilter(std::unique_ptr<MessageFilter> filter);

  // After this call the object is in an invalid state and shouldn't be reused.
  ScopedInterfaceEndpointHandle PassHandle();

  // Raises an error on the underlying message pipe. It disconnects the pipe
  // and notifies all interfaces running on this pipe.
  void RaiseError();

  void CloseWithReason(uint32_t custom_reason, const std::string& description);

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

  // Implementations used by both SendControlMessage* and Accept* above.
  bool SendMessage(Message* message, bool is_control_message);
  bool SendMessageWithResponder(Message* message,
                                bool is_control_message,
                                std::unique_ptr<MessageReceiver> responder);

  // The following methods are called by the router. They must be called
  // outside of the router's lock.

  // NOTE: |message| must have passed message header validation.
  bool HandleIncomingMessage(Message* message);
  void NotifyError(const base::Optional<DisconnectReason>& reason);

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

  void force_outgoing_messages_async(bool force) {
    force_outgoing_messages_async_ = force;
  }

#if DCHECK_IS_ON()
  void SetNextCallLocation(const base::Location& location) {
    next_call_location_ = location;
  }
#endif

 private:
  // Maps from the id of a response to the MessageReceiver that handles the
  // response.
  using AsyncResponderMap =
      std::map<uint64_t, std::unique_ptr<MessageReceiver>>;

  struct SyncResponseInfo {
   public:
    explicit SyncResponseInfo(bool* in_response_received);
    ~SyncResponseInfo();

    Message response;

    // Points to a stack-allocated variable.
    bool* response_received;

   private:
    DISALLOW_COPY_AND_ASSIGN(SyncResponseInfo);
  };

  using SyncResponseMap = std::map<uint64_t, std::unique_ptr<SyncResponseInfo>>;

  // Used as the sink for |payload_validator_| and forwards messages to
  // HandleValidatedMessage().
  class HandleIncomingMessageThunk : public MessageReceiver {
   public:
    explicit HandleIncomingMessageThunk(InterfaceEndpointClient* owner);
    ~HandleIncomingMessageThunk() override;

    // MessageReceiver implementation:
    bool Accept(Message* message) override;

   private:
    InterfaceEndpointClient* const owner_;

    DISALLOW_COPY_AND_ASSIGN(HandleIncomingMessageThunk);
  };

  void InitControllerIfNecessary();

  void OnAssociationEvent(
      ScopedInterfaceEndpointHandle::AssociationEvent event);

  bool HandleValidatedMessage(Message* message);

  const bool expect_sync_requests_ = false;

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
  base::Optional<base::TimeDelta> idle_timeout_;

  // The current idle timer, valid only while we're idle. If this fires, we send
  // a NotifyIdle to our peer.
  base::Optional<base::OneShotTimer> notify_idle_timer_;

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
  InterfaceEndpointController* controller_ = nullptr;

  MessageReceiverWithResponderStatus* const incoming_receiver_ = nullptr;
  HandleIncomingMessageThunk thunk_{this};
  MessageDispatcher dispatcher_;

  AsyncResponderMap async_responders_;
  SyncResponseMap sync_responses_;

  uint64_t next_request_id_ = 1;

  base::OnceClosure error_handler_;
  ConnectionErrorWithReasonCallback error_with_reason_handler_;
  bool encountered_error_ = false;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  internal::ControlMessageProxy control_message_proxy_{this};
  internal::ControlMessageHandler control_message_handler_;
  const char* interface_name_;

#if DCHECK_IS_ON()
  // The code location of the the most recent call into a method on this
  // interface endpoint. This is set *after* the call but *before* any message
  // is actually transmitted for it.
  base::Location next_call_location_;
#endif

  // If set to |true|, the endpoint ignores the sync flag when sending messages.
  // This means that all messages are sent as if they were async, and all
  // incoming replies are treated as if they replied to an async message. It is
  // NOT appropriate to call generated sync method signatures (i.e. mojom
  // interface methods with output arguments) on such endpoints.
  //
  // This exists only to facilitate APIs forwarding opaque sync messages through
  // the endpoint from some other sequence which blocks on the reply, such as
  // with sync calls on a SharedRemote.
  bool force_outgoing_messages_async_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterfaceEndpointClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterfaceEndpointClient);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CLIENT_H_
