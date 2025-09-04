// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/webauthn/remote_webauthn_native_messaging_host.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_host_services_client.h"
#include "remoting/host/mojom/webauthn_proxy.mojom.h"
#include "remoting/host/native_messaging/native_messaging_constants.h"
#include "remoting/host/native_messaging/native_messaging_helpers.h"
#include "remoting/host/webauthn/desktop_session_type_util.h"
#include "remoting/host/webauthn/remote_webauthn_constants.h"

namespace remoting {

namespace {

// Delay to wait for a response to `remote_.QueryVersion()`. If we don't get a
// response after it, we will treat the IPC connection as disconnected.
constexpr base::TimeDelta kQueryRemoteStateTimeout = base::Seconds(5);

base::Value::Dict CreateWebAuthnExceptionDetailsDict(
    const std::string& name,
    const std::string& message) {
  return base::Value::Dict()
      .Set(kWebAuthnErrorNameKey, name)
      .Set(kWebAuthnErrorMessageKey, message);
}

base::Value::Dict MojoErrorToErrorDict(
    const mojom::WebAuthnExceptionDetailsPtr& mojo_error) {
  return CreateWebAuthnExceptionDetailsDict(mojo_error->name,
                                            mojo_error->message);
}

}  // namespace

RemoteWebAuthnNativeMessagingHost::RemoteWebAuthnNativeMessagingHost(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : RemoteWebAuthnNativeMessagingHost(
          std::make_unique<ChromotingHostServicesClient>(),
          task_runner) {}

RemoteWebAuthnNativeMessagingHost::RemoteWebAuthnNativeMessagingHost(
    std::unique_ptr<ChromotingHostServicesProvider> host_service_api_client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      host_service_api_client_(std::move(host_service_api_client)) {
  request_cancellers_.set_disconnect_handler(base::BindRepeating(
      &RemoteWebAuthnNativeMessagingHost::OnRequestCancellerDisconnected,
      base::Unretained(this)));
}

RemoteWebAuthnNativeMessagingHost::~RemoteWebAuthnNativeMessagingHost() {
  DCHECK(task_runner_->BelongsToCurrentThread());

#if !BUILDFLAG(IS_CHROMEOS)
  // This makes sure the log messages below get sent to the extension before the
  // caller sequence gets terminated.
  log_message_handler_->set_log_synchronously_if_possible(true);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  if (!id_to_request_canceller_.empty()) {
    LOG(WARNING) << id_to_request_canceller_.size()
                 << "Requests are still pending at destruction.";
  }
  HOST_LOG << "Remote WebAuthn native messaging host is being terminated";
}

void RemoteWebAuthnNativeMessagingHost::OnMessage(const std::string& message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::string type;
  base::Value::Dict request;
  if (!ParseNativeMessageJson(message, type, request)) {
    return;
  }

  std::optional<base::Value::Dict> response =
      CreateNativeMessageResponse(request);
  if (!response.has_value()) {
    return;
  }

  if (type == kHelloMessage) {
    ProcessHello(std::move(*response));
    return;
  } else if (type == kGetRemoteStateMessageType) {
    ProcessGetRemoteState(std::move(*response));
    return;
  }

  void (RemoteWebAuthnNativeMessagingHost::*fn)(
      const base::Value::Dict& request, base::Value::Dict response);
  if (type == kIsUvpaaMessageType) {
    fn = &RemoteWebAuthnNativeMessagingHost::ProcessIsUvpaa;
  } else if (type == kCreateMessageType) {
    fn = &RemoteWebAuthnNativeMessagingHost::ProcessCreate;
  } else if (type == kGetMessageType) {
    fn = &RemoteWebAuthnNativeMessagingHost::ProcessGet;
  } else if (type == kCancelMessageType) {
    fn = &RemoteWebAuthnNativeMessagingHost::ProcessCancel;
  } else {
    LOG(ERROR) << "Unsupported request type: " << type;
    return;
  }
  EnsureIpcConnectionThenProcess(base::BindOnce(
      fn, base::Unretained(this), std::move(request), std::move(*response)));
}

void RemoteWebAuthnNativeMessagingHost::Start(
    extensions::NativeMessageHost::Client* client) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  client_ = client;
#if !BUILDFLAG(IS_CHROMEOS)
  log_message_handler_ =
      std::make_unique<LogMessageHandler>(base::BindRepeating(
          &RemoteWebAuthnNativeMessagingHost::SendMessageToClient,
          base::Unretained(this)));
#endif  // !BUILDFLAG(IS_CHROMEOS)
  HOST_LOG << "Remote WebAuthn native messaging host has started";
}

scoped_refptr<base::SingleThreadTaskRunner>
RemoteWebAuthnNativeMessagingHost::task_runner() const {
  return task_runner_;
}

void RemoteWebAuthnNativeMessagingHost::ProcessHello(
    base::Value::Dict response) {
  // Hello request: {id: string, type: 'hello'}
  // Hello response: {id: string, type: 'helloResponse', hostVersion: string}

  DCHECK(task_runner_->BelongsToCurrentThread());

  ProcessNativeMessageHelloResponse(response);
  SendMessageToClient(std::move(response));
}

void RemoteWebAuthnNativeMessagingHost::ProcessIsUvpaa(
    const base::Value::Dict& request,
    base::Value::Dict response) {
  // IsUvpaa request: {id: string, type: 'isUvpaa'}
  // IsUvpaa response:
  //   {id: string, type: 'isUvpaaResponse', isAvailable: boolean}

  DCHECK(task_runner_->BelongsToCurrentThread());

  remote_->IsUserVerifyingPlatformAuthenticatorAvailable(
      base::BindOnce(&RemoteWebAuthnNativeMessagingHost::OnIsUvpaaResponse,
                     base::Unretained(this), std::move(response)));
}

void RemoteWebAuthnNativeMessagingHost::ProcessCreate(
    const base::Value::Dict& request,
    base::Value::Dict response) {
  // Create request: {id: string, type: 'create', requestData: string}
  // Create response: {
  //   id: string, type: 'createResponse', responseData?: string,
  //   error?: {name: string, message: string}}

  DCHECK(task_runner_->BelongsToCurrentThread());

  const base::Value* message_id = FindMessageIdOrSendError(response);
  if (!message_id) {
    return;
  }
  const std::string* request_data =
      FindRequestDataOrSendError(request, kCreateRequestDataKey, response);
  if (!request_data) {
    return;
  }

  remote_->Create(
      *request_data, AddRequestCanceller(message_id->Clone()),
      base::BindOnce(&RemoteWebAuthnNativeMessagingHost::OnCreateResponse,
                     base::Unretained(this), std::move(response)));
}

void RemoteWebAuthnNativeMessagingHost::ProcessGet(
    const base::Value::Dict& request,
    base::Value::Dict response) {
  // Get request: {id: string, type: 'get', requestData: string}
  // Get response: {
  //   id: string, type: 'getResponse', responseData?: string,
  //   error?: {name: string, message: string}}

  DCHECK(task_runner_->BelongsToCurrentThread());

  const base::Value* message_id = FindMessageIdOrSendError(response);
  if (!message_id) {
    return;
  }
  const std::string* request_data =
      FindRequestDataOrSendError(request, kGetRequestDataKey, response);
  if (!request_data) {
    return;
  }

  remote_->Get(*request_data, AddRequestCanceller(message_id->Clone()),
               base::BindOnce(&RemoteWebAuthnNativeMessagingHost::OnGetResponse,
                              base::Unretained(this), std::move(response)));
}

void RemoteWebAuthnNativeMessagingHost::ProcessCancel(
    const base::Value::Dict& request,
    base::Value::Dict response) {
  // Cancel request: {id: string, type: 'cancel'}
  // Cancel response:
  //   {id: string, type: 'cancelResponse', wasCanceled: boolean}

  const base::Value* message_id = request.Find(kMessageId);
  if (!message_id) {
    LOG(ERROR) << "Message ID not found in cancel request.";
    response.Set(kCancelResponseWasCanceledKey, false);
    SendMessageToClient(std::move(response));
    return;
  }

  auto it = id_to_request_canceller_.find(*message_id);
  if (it == id_to_request_canceller_.end()) {
    LOG(ERROR) << "No cancelable request found for message ID " << *message_id;
    response.Set(kCancelResponseWasCanceledKey, false);
    SendMessageToClient(std::move(response));
    return;
  }

  auto* canceller = request_cancellers_.Get(it->second);
  CHECK(canceller);
  canceller->Cancel(
      base::BindOnce(&RemoteWebAuthnNativeMessagingHost::OnCancelResponse,
                     base::Unretained(this), std::move(response)));
}

void RemoteWebAuthnNativeMessagingHost::ProcessGetRemoteState(
    base::Value::Dict response) {
  // GetRemoteState request: {id: string, type: 'getRemoteState'}
  // GetRemoteState response:
  //   {id: string, type: 'getRemoteStateResponse', isRemoted: boolean,
  //    desktopSessionType: 'unspecified'|'remote-only'|'local-only'}

  DCHECK(task_runner_->BelongsToCurrentThread());

  // The browser sends getRemoteState whenever it detects a remote state change.
  // It is possible that a stale state is received after a second getRemoteState
  // request is received, so we need to make one query at a time to prevent race
  // conditions.
  pending_get_remote_state_requests_.push(std::move(response));
  if (pending_get_remote_state_requests_.size() == 1) {
    QueryNextRemoteState();
  }
  // Otherwise it means there is already a pending remote state request.
}

void RemoteWebAuthnNativeMessagingHost::OnQueryVersionResult(uint32_t version) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  query_remote_state_timeout_timer_.Stop();
  is_connection_valid_ = true;
  while (!pending_process_requests_.empty()) {
    std::move(pending_process_requests_.front()).Run();
    pending_process_requests_.pop();
  }

  if (!pending_get_remote_state_requests_.empty()) {
    SendNextRemoteState(true);
  }
}

void RemoteWebAuthnNativeMessagingHost::OnQueryNextRemoteStateTimeout() {
  LOG(ERROR) << "Timed out waiting for the response of QueryVersion";
  OnIpcDisconnected();
}

void RemoteWebAuthnNativeMessagingHost::OnIpcDisconnected() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  query_remote_state_timeout_timer_.Stop();
  is_connection_valid_ = false;
  remote_.reset();
  // base::queue does not have a clear() method.
  base::queue<base::OnceClosure> empty_queue;
  pending_process_requests_.swap(empty_queue);
  id_to_request_canceller_.clear();
  request_cancellers_.Clear();

  if (!pending_get_remote_state_requests_.empty()) {
    SendNextRemoteState(false);
  } else {
    SendClientDisconnectedMessage();
  }
}

void RemoteWebAuthnNativeMessagingHost::OnIsUvpaaResponse(
    base::Value::Dict response,
    bool is_available) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  response.Set(kIsUvpaaResponseIsAvailableKey, is_available);
  SendMessageToClient(std::move(response));
}

void RemoteWebAuthnNativeMessagingHost::OnCreateResponse(
    base::Value::Dict response,
    mojom::WebAuthnCreateResponsePtr remote_response) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If |remote_response| is null, it means that the remote create() call has
  // yielded `null`, which is still a valid response according to the spec. In
  // this case we just send back an empty create response.
  if (!remote_response.is_null()) {
    switch (remote_response->which()) {
      case mojom::WebAuthnCreateResponse::Tag::kErrorDetails:
        response.Set(
            kWebAuthnErrorKey,
            MojoErrorToErrorDict(remote_response->get_error_details()));
        break;
      case mojom::WebAuthnCreateResponse::Tag::kResponseData:
        response.Set(kCreateResponseDataKey,
                     remote_response->get_response_data());
        break;
      default:
        NOTREACHED() << "Unexpected create response tag: "
                     << static_cast<uint32_t>(remote_response->which());
    }
  }

  const base::Value* message_id = response.Find(kMessageId);
  if (message_id) {
    RemoveRequestCancellerByMessageId(*message_id);
  }

  SendMessageToClient(std::move(response));
}

void RemoteWebAuthnNativeMessagingHost::OnGetResponse(
    base::Value::Dict response,
    mojom::WebAuthnGetResponsePtr remote_response) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // If |remote_response| is null, it means that the remote get() call has
  // yielded `null`, which is still a valid response according to the spec. In
  // this case we just send back an empty get response.
  if (!remote_response.is_null()) {
    switch (remote_response->which()) {
      case mojom::WebAuthnGetResponse::Tag::kErrorDetails:
        response.Set(
            kWebAuthnErrorKey,
            MojoErrorToErrorDict(remote_response->get_error_details()));
        break;
      case mojom::WebAuthnGetResponse::Tag::kResponseData:
        response.Set(kGetResponseDataKey, remote_response->get_response_data());
        break;
      default:
        NOTREACHED() << "Unexpected get response tag: "
                     << static_cast<uint32_t>(remote_response->which());
    }
  }

  const base::Value* message_id = response.Find(kMessageId);
  if (message_id) {
    RemoveRequestCancellerByMessageId(*message_id);
  }

  SendMessageToClient(std::move(response));
}

void RemoteWebAuthnNativeMessagingHost::OnCancelResponse(
    base::Value::Dict response,
    bool was_canceled) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  const base::Value* message_id = response.Find(kMessageId);
  if (message_id) {
    RemoveRequestCancellerByMessageId(*message_id);
  }

  response.Set(kCancelResponseWasCanceledKey, was_canceled);
  SendMessageToClient(std::move(response));
}

void RemoteWebAuthnNativeMessagingHost::QueryNextRemoteState() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (!EnsureRemoteBound()) {
    OnIpcDisconnected();
    return;
  }

  query_remote_state_timeout_timer_.Start(
      FROM_HERE, kQueryRemoteStateTimeout, this,
      &RemoteWebAuthnNativeMessagingHost::OnQueryNextRemoteStateTimeout);

  // QueryVersion() is simply used to determine if the receiving end actually
  // accepts the connection. If it doesn't, then the callback will be silently
  // dropped, and OnIpcDisconnected() will be called instead.
  remote_.QueryVersion(
      base::BindOnce(&RemoteWebAuthnNativeMessagingHost::OnQueryVersionResult,
                     base::Unretained(this)));
}

void RemoteWebAuthnNativeMessagingHost::SendNextRemoteState(bool is_remoted) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!pending_get_remote_state_requests_.empty());

  auto response = std::move(pending_get_remote_state_requests_.front());
  pending_get_remote_state_requests_.pop();

  response.Set(kGetRemoteStateResponseIsRemotedKey, is_remoted);
  std::string desktop_session_type;
  switch (GetDesktopSessionType()) {
    case DesktopSessionType::UNSPECIFIED:
      desktop_session_type = "unspecified";
      break;
    case DesktopSessionType::REMOTE_ONLY:
      desktop_session_type = "remote-only";
      break;
    case DesktopSessionType::LOCAL_ONLY:
      desktop_session_type = "local-only";
      break;
  }
  response.Set(kGetRemoteStateResponseDesktopSessionTypeKey,
               desktop_session_type);
  SendMessageToClient(std::move(response));
  if (!pending_get_remote_state_requests_.empty()) {
    QueryNextRemoteState();
  }
}

void RemoteWebAuthnNativeMessagingHost::EnsureIpcConnectionThenProcess(
    base::OnceClosure closure) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!closure.is_null());

  if (is_connection_valid_) {
    std::move(closure).Run();
    return;
  }

  pending_process_requests_.push(std::move(closure));

  QueryNextRemoteState();
}

bool RemoteWebAuthnNativeMessagingHost::EnsureRemoteBound() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (remote_.is_bound()) {
    return true;
  }

  auto disconnect_handler =
      base::BindRepeating(&RemoteWebAuthnNativeMessagingHost::OnIpcDisconnected,
                          base::Unretained(this));
  // There is a bug in Mojo, such that if the host rejects binding of session
  // services, there is a chance that binding of WebAuthnProxy appears to be
  // successful and the disconnect handler of `remote_` is never called, so
  // `remote_` will remain invalid forever.
  // The disconnect handler of session services is still called, so we set a
  // disconnect handler on it.
  // See https://crbug.com/425759818#comment8 for more context.
  host_service_api_client_->set_disconnect_handler(disconnect_handler);
  auto* api = host_service_api_client_->GetSessionServices();
  if (!api) {
    return false;
  }
  api->BindWebAuthnProxy(remote_.BindNewPipeAndPassReceiver());
  remote_.set_disconnect_handler(disconnect_handler);
  return true;
}

void RemoteWebAuthnNativeMessagingHost::SendMessageToClient(
    base::Value::Dict message) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  std::optional<std::string> message_json = base::WriteJson(message);
  if (!message_json.has_value()) {
    LOG(ERROR) << "Failed to write message to JSON";
    return;
  }
  client_->PostMessageFromNativeHost(message_json.value());
}

const base::Value* RemoteWebAuthnNativeMessagingHost::FindMessageIdOrSendError(
    base::Value::Dict& response) {
  const base::Value* message_id = response.Find(kMessageId);
  if (message_id) {
    return message_id;
  }
  response.Set(kWebAuthnErrorKey,
               CreateWebAuthnExceptionDetailsDict(
                   "NotSupportedError", "Message ID not found in request."));
  SendMessageToClient(std::move(response));
  return nullptr;
}

const std::string*
RemoteWebAuthnNativeMessagingHost::FindRequestDataOrSendError(
    const base::Value::Dict& request,
    const std::string& request_data_key,
    base::Value::Dict& response) {
  const std::string* request_data = request.FindString(request_data_key);
  if (request_data) {
    return request_data;
  }
  response.Set(
      kWebAuthnErrorKey,
      CreateWebAuthnExceptionDetailsDict(
          "NotSupportedError", "Request data not found in the request."));
  SendMessageToClient(std::move(response));
  return nullptr;
}

mojo::PendingReceiver<mojom::WebAuthnRequestCanceller>
RemoteWebAuthnNativeMessagingHost::AddRequestCanceller(base::Value message_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  mojo::PendingRemote<mojom::WebAuthnRequestCanceller>
      pending_request_canceller;
  auto request_canceller_receiver =
      pending_request_canceller.InitWithNewPipeAndPassReceiver();
  id_to_request_canceller_.emplace(
      std::move(message_id),
      request_cancellers_.Add(std::move(pending_request_canceller)));
  return request_canceller_receiver;
}

void RemoteWebAuthnNativeMessagingHost::RemoveRequestCancellerByMessageId(
    const base::Value& message_id) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto it = id_to_request_canceller_.find(message_id);
  if (it != id_to_request_canceller_.end()) {
    request_cancellers_.Remove(it->second);
    id_to_request_canceller_.erase(it);
  } else {
    // This may happen, say if the request canceller is disconnected before the
    // create/get response is received, so we just verbose-log it.
    VLOG(1) << "Cannot find receiver for message ID " << message_id;
  }
}

void RemoteWebAuthnNativeMessagingHost::OnRequestCancellerDisconnected(
    mojo::RemoteSetElementId disconnecting_canceller) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  auto it = std::ranges::find(id_to_request_canceller_, disconnecting_canceller,
                              &IdToRequestMap::value_type::second);
  if (it != id_to_request_canceller_.end()) {
    id_to_request_canceller_.erase(it);
  }

  if (on_request_canceller_disconnected_for_testing_) {
    on_request_canceller_disconnected_for_testing_.Run();
  }
}

void RemoteWebAuthnNativeMessagingHost::SendClientDisconnectedMessage() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  base::Value::Dict message;
  message.Set(kMessageType, kClientDisconnectedMessageType);
  SendMessageToClient(std::move(message));
}

}  // namespace remoting
