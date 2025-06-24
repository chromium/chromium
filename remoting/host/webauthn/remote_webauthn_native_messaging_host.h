// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_NATIVE_MESSAGING_HOST_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "remoting/host/chromoting_host_services_provider.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/host/native_messaging/log_message_handler.h"

namespace remoting {

// Native messaging host for handling remote authentication requests and sending
// them to the remoting host process via mojo.
class RemoteWebAuthnNativeMessagingHost final
    : public extensions::NativeMessageHost {
 public:
  explicit RemoteWebAuthnNativeMessagingHost(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  RemoteWebAuthnNativeMessagingHost(const RemoteWebAuthnNativeMessagingHost&) =
      delete;
  RemoteWebAuthnNativeMessagingHost& operator=(
      const RemoteWebAuthnNativeMessagingHost&) = delete;
  ~RemoteWebAuthnNativeMessagingHost() override;

  void OnMessage(const std::string& message) override;
  void Start(extensions::NativeMessageHost::Client* client) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const override;

 private:
  friend class RemoteWebAuthnNativeMessagingHostTest;

  using IdToRequestMap = base::flat_map<base::Value, mojo::RemoteSetElementId>;

  RemoteWebAuthnNativeMessagingHost(
      std::unique_ptr<ChromotingHostServicesProvider> host_service_api_client,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void ProcessHello(base::Value::Dict response);
  void ProcessGetRemoteState(base::Value::Dict response);

  // The following methods need to be called with
  // EnsureIpcConnectionThenProcess().
  void ProcessIsUvpaa(const base::Value::Dict& request,
                      base::Value::Dict response);
  void ProcessCreate(const base::Value::Dict& request,
                     base::Value::Dict response);
  void ProcessGet(const base::Value::Dict& request, base::Value::Dict response);
  void ProcessCancel(const base::Value::Dict& request,
                     base::Value::Dict response);

  void OnQueryVersionResult(uint32_t version);
  void OnQueryNextRemoteStateTimeout();
  void OnIpcDisconnected();
  void OnIsUvpaaResponse(base::Value::Dict response, bool is_available);
  void OnCreateResponse(base::Value::Dict response,
                        mojom::WebAuthnCreateResponsePtr remote_response);
  void OnGetResponse(base::Value::Dict response,
                     mojom::WebAuthnGetResponsePtr remote_response);
  void OnCancelResponse(base::Value::Dict response, bool was_canceled);

  // Query the next remote state. There are three possible outcomes:
  // 1. Remote state is connected (receiver responds with the version):
  //    * All pending requests queued by EnsureIpcConnectionThenProcess() will
  //      be executed.
  //    * The current remote state will be sent to the browser if there is at
  //      least one pending getRemoteState request.
  //    * Call QueryNextRemoteState again if there are still remaining
  //      getRemoteState requests. See comments in ProcessGetRemoteState().
  // 2. Remote state is disconnected (receiver is closed remotely, or IPC is
  //    disconnected; the disconnect handler will be called):
  //    * All pending requests queued by EnsureIpcConnectionThenProcess() will
  //      be dropped.
  //    * Client disconnected message will be sent to the browser if there is no
  //      pending getRemoteState request.
  //    * The current remote state will be sent to the browser if there is at
  //      least one pending getRemoteState request.
  //    * Call QueryNextRemoteState again if there are still remaining
  //      getRemoteState requests. See comments in ProcessGetRemoteState().
  // 3. Timed out waiting for the QueryVersion result. This happens if the
  //    pending receiver isn't bound or dropped on the host side, possibly due
  //    to a bug in Mojo:
  //    This is treated the same as the remote state is disconnected.
  void QueryNextRemoteState();
  void SendNextRemoteState(bool is_remoted);

  // Attempts to connect to the IPC server if the connection has not been
  // established. Calls `closure` with `request` and `response` if the IPC is
  // established, otherwise drops the request and just sends a client
  // disconnected message to the browser.
  void EnsureIpcConnectionThenProcess(base::OnceClosure closure);

  // Attempts to bind `remote_` if it has not been bound. Returns a boolean
  // indicating whether the remote was successfully bound.
  // Note that a remote is bound doesn't necessarily mean that the connection
  // is valid. Please check `is_connection_valid_` explicitly.
  bool EnsureRemoteBound();

  void SendMessageToClient(base::Value::Dict message);

  // Finds and returns the message ID from |response|. If message ID is not
  // found, |response| will be attached with a WebAuthn error dict and sent to
  // the NMH client, and `nullptr` will be returned.
  const base::Value* FindMessageIdOrSendError(base::Value::Dict& response);

  // Finds and returns request[request_data_key]. If request_data_key is not
  // found, |response| will be attached with a WebAuthn error dict and sent to
  // the NMH client, and `nullptr` will be returned.
  const std::string* FindRequestDataOrSendError(
      const base::Value::Dict& request,
      const std::string& request_data_key,
      base::Value::Dict& response);

  mojo::PendingReceiver<mojom::WebAuthnRequestCanceller> AddRequestCanceller(
      base::Value message_id);
  void RemoveRequestCancellerByMessageId(const base::Value& message_id);
  void OnRequestCancellerDisconnected(
      mojo::RemoteSetElementId disconnecting_canceller);

  // Sends a clientDisconnected message to the extension, so it can detach from
  // the WebAuthn proxy API and clean up pending requests.
  void SendClientDisconnectedMessage();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<ChromotingHostServicesProvider> host_service_api_client_;

  // `remote_` is bound whenever a pending receiver is created. This does not
  // necessarily mean that the IPC connection is valid, unless the pending
  // receiver is received and bound to a receiver by the host, otherwise
  // invocations will be silently dropped. It is only valid if
  // `is_connection_valid_` is true, which is only true if the response of
  // remote_.QueryVersion() has been received.
  mojo::Remote<mojom::WebAuthnProxy> remote_;
  bool is_connection_valid_ = false;

  base::queue<base::OnceClosure> pending_process_requests_;
  mojo::RemoteSet<mojom::WebAuthnRequestCanceller> request_cancellers_;
  IdToRequestMap id_to_request_canceller_;

  // Only available after Start() is called.
  raw_ptr<extensions::NativeMessageHost::Client> client_ = nullptr;
  std::unique_ptr<LogMessageHandler> log_message_handler_;

  base::RepeatingClosure on_request_canceller_disconnected_for_testing_;

  base::OneShotTimer query_remote_state_timeout_timer_;

  // The elements are actually the partial response objects for getRemoteState,
  // i.e. {id: string}.
  base::queue<base::Value::Dict> pending_get_remote_state_requests_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WEBAUTHN_REMOTE_WEBAUTHN_NATIVE_MESSAGING_HOST_H_
