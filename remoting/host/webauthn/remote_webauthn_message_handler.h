// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_MESSAGE_HANDLER_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_MESSAGE_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/protocol/named_message_pipe_handler.h"

namespace remoting {

namespace protocol {
class RemoteWebAuthn_CancelResponse;
class RemoteWebAuthn_CreateResponse;
class RemoteWebAuthn_GetResponse;
class RemoteWebAuthn_IsUvpaaResponse;
}  // namespace protocol

class RemoteWebAuthnStateChangeNotifier;

class RemoteWebAuthnMessageHandler final
    : public mojom::WebAuthnProxy,
      public mojom::WebAuthnRequestCanceller,
      public protocol::NamedMessagePipeHandler {
 public:
  RemoteWebAuthnMessageHandler(
      const std::string& name,
      std::unique_ptr<protocol::MessagePipe> pipe,
      std::unique_ptr<RemoteWebAuthnStateChangeNotifier> state_change_notifier);
  RemoteWebAuthnMessageHandler(const RemoteWebAuthnMessageHandler&) = delete;
  RemoteWebAuthnMessageHandler& operator=(const RemoteWebAuthnMessageHandler&) =
      delete;
  ~RemoteWebAuthnMessageHandler() override;

  // protocol::NamedMessagePipeHandler implementation.
  void OnConnected() override;
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;
  void OnDisconnecting() override;

  // mojom::WebAuthnProxy implementation.
  void IsUserVerifyingPlatformAuthenticatorAvailable(
      IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) override;
  void Create(
      const std::string& request_data,
      mojo::PendingReceiver<mojom::WebAuthnRequestCanceller> request_canceller,
      CreateCallback callback) override;
  void Get(
      const std::string& request_data,
      mojo::PendingReceiver<mojom::WebAuthnRequestCanceller> request_canceller,
      GetCallback callback) override;

  // mojom::WebAuthnRequestCanceller implementation.
  void Cancel(CancelCallback callback) override;

  void AddReceiver(mojo::PendingReceiver<mojom::WebAuthnProxy> receiver);
  void ClearReceivers();

  // Notifies the WebAuthn proxy extension that the availablitiy of WebAuthn
  // proxying may have changed.
  void NotifyWebAuthnStateChange();

  base::WeakPtr<RemoteWebAuthnMessageHandler> GetWeakPtr();

 private:
  template <typename CallbackType>
  using CallbackMap = base::flat_map<uint64_t, CallbackType>;

  friend class RemoteWebAuthnMessageHandlerTest;

  void OnReceiverDisconnected();
  void OnIsUvpaaResponse(
      uint64_t id,
      const protocol::RemoteWebAuthn_IsUvpaaResponse& response);
  void OnCreateResponse(
      uint64_t id,
      const protocol::RemoteWebAuthn_CreateResponse& response);
  void OnGetResponse(uint64_t id,
                     const protocol::RemoteWebAuthn_GetResponse& response);
  void OnCancelResponse(
      uint64_t id,
      const protocol::RemoteWebAuthn_CancelResponse& response);

  uint64_t AssignNextMessageId();

  void AddRequestCanceller(
      uint64_t message_id,
      mojo::PendingReceiver<mojom::WebAuthnRequestCanceller> request_canceller);
  void RemoveRequestCancellerByMessageId(uint64_t message_id);
  void OnRequestCancellerDisconnected();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<RemoteWebAuthnStateChangeNotifier> state_change_notifier_;
  mojo::ReceiverSet<mojom::WebAuthnProxy> receiver_set_;

  // message ID => mojo callback mappings.
  CallbackMap<IsUserVerifyingPlatformAuthenticatorAvailableCallback>
      is_uvpaa_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
  CallbackMap<CreateCallback> create_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);
  CallbackMap<GetCallback> get_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);
  CallbackMap<CancelCallback> cancel_callbacks_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The receiver context is the message ID associated with the request to be
  // canceled.
  mojo::ReceiverSet<mojom::WebAuthnRequestCanceller, uint64_t>
      request_cancellers_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_map<uint64_t, mojo::ReceiverId> message_id_to_request_canceller_
      GUARDED_BY_CONTEXT(sequence_checker_);

  uint64_t current_message_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0u;

  base::WeakPtrFactory<RemoteWebAuthnMessageHandler> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_MESSAGE_HANDLER_H_
