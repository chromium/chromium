// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/display_source/wifi_display/wifi_display_session_service_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/display_source/display_source_connection_delegate_factory.h"

namespace {
const char kErrorCannotHaveMultipleSessions[] =
    "Multiple Wi-Fi Display sessions are not supported";
const char kErrorSinkNotAvailable[] = "The sink is not available";
}  // namespace

namespace extensions {

WiFiDisplaySessionServiceImpl::WiFiDisplaySessionServiceImpl(
    DisplaySourceConnectionDelegate* delegate)
    : delegate_(delegate),
      sink_state_(SINK_STATE_NONE),
      sink_id_(DisplaySourceConnectionDelegate::kInvalidSinkId),
      weak_factory_(this) {
  delegate_->AddObserver(this);
}

WiFiDisplaySessionServiceImpl::~WiFiDisplaySessionServiceImpl() {
  delegate_->RemoveObserver(this);
  Disconnect();
}

// static
void WiFiDisplaySessionServiceImpl::BindToReceiver(
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<WiFiDisplaySessionService> receiver,
    content::RenderFrameHost* render_frame_host) {
  DisplaySourceConnectionDelegate* delegate =
      DisplaySourceConnectionDelegateFactory::GetForBrowserContext(
          browser_context);
  CHECK(delegate);
  auto* impl = new WiFiDisplaySessionServiceImpl(delegate);
  impl->receiver_ =
      mojo::MakeSelfOwnedReceiver(base::WrapUnique(impl), std::move(receiver));
}

void WiFiDisplaySessionServiceImpl::SetClient(
    mojo::PendingRemote<mojom::WiFiDisplaySessionServiceClient> client) {
  DCHECK(client);
  DCHECK(!client_);
  client_.Bind(std::move(client));
  client_.set_disconnect_handler(
      base::Bind(&WiFiDisplaySessionServiceImpl::OnClientConnectionError,
                 weak_factory_.GetWeakPtr()));
}

void WiFiDisplaySessionServiceImpl::Connect(int32_t sink_id,
                                            int32_t auth_method,
                                            const std::string& auth_data) {
  DCHECK(client_);
  // We support only one Wi-Fi Display session at a time.
  if (delegate_->connection()) {
    client_->OnConnectRequestHandled(false, kErrorCannotHaveMultipleSessions);
    return;
  }

  const DisplaySourceSinkInfoList& sinks = delegate_->last_found_sinks();
  auto found = std::find_if(sinks.begin(), sinks.end(),
                            [sink_id](const DisplaySourceSinkInfo& sink) {
                              return sink.id == sink_id;
                            });
  if (found == sinks.end() || found->state != SINK_STATE_DISCONNECTED) {
    client_->OnConnectRequestHandled(false, kErrorSinkNotAvailable);
    return;
  }
  AuthenticationInfo auth_info;
  if (auth_method != AUTHENTICATION_METHOD_NONE) {
    DCHECK(auth_method <= AUTHENTICATION_METHOD_LAST);
    auth_info.method = static_cast<AuthenticationMethod>(auth_method);
    auth_info.data = std::unique_ptr<std::string>(new std::string(auth_data));
  }
  auto on_error = base::Bind(&WiFiDisplaySessionServiceImpl::OnConnectFailed,
                             weak_factory_.GetWeakPtr(), sink_id);
  delegate_->Connect(sink_id, auth_info, on_error);
  sink_id_ = sink_id;
  sink_state_ = found->state;
  DCHECK(sink_state_ == SINK_STATE_CONNECTING);
  client_->OnConnectRequestHandled(true, "");
}

void WiFiDisplaySessionServiceImpl::Disconnect() {
  if (sink_id_ == DisplaySourceConnectionDelegate::kInvalidSinkId) {
    // The connection might drop before this call has arrived.
    // Renderer should have been notified already.
    return;
  }

  const DisplaySourceSinkInfoList& sinks = delegate_->last_found_sinks();
  auto found = std::find_if(sinks.begin(), sinks.end(),
                            [this](const DisplaySourceSinkInfo& sink) {
                              return sink.id == sink_id_;
                            });
  DCHECK(found != sinks.end());
  DCHECK(found->state == SINK_STATE_CONNECTED ||
         found->state == SINK_STATE_CONNECTING);

  auto on_error = base::Bind(&WiFiDisplaySessionServiceImpl::OnDisconnectFailed,
                             weak_factory_.GetWeakPtr(), sink_id_);
  delegate_->Disconnect(on_error);
}

void WiFiDisplaySessionServiceImpl::SendMessage(const std::string& message) {
  if (sink_id_ == DisplaySourceConnectionDelegate::kInvalidSinkId) {
    // The connection might drop before this call has arrived.
    return;
  }
  auto connection = delegate_->connection();
  DCHECK(connection);
  DCHECK_EQ(sink_id_, connection->GetConnectedSink().id);
  connection->SendMessage(message);
}

void WiFiDisplaySessionServiceImpl::OnSinkMessage(const std::string& message) {
  DCHECK(delegate_->connection());
  DCHECK_NE(sink_id_, DisplaySourceConnectionDelegate::kInvalidSinkId);
  DCHECK(client_);
  client_->OnMessage(message);
}

void WiFiDisplaySessionServiceImpl::OnSinksUpdated(
    const DisplaySourceSinkInfoList& sinks) {
  if (sink_id_ == DisplaySourceConnectionDelegate::kInvalidSinkId)
    return;
  // The initialized sink id means that the client should have
  // been initialized as well.
  DCHECK(client_);
  auto found = std::find_if(sinks.begin(), sinks.end(),
                            [this](const DisplaySourceSinkInfo& sink) {
                              return sink.id == sink_id_;
                            });
  if (found == sinks.end()) {
    client_->OnError(ERROR_TYPE_CONNECTION_ERROR, "The sink has disappeared");
    client_->OnTerminated();
    sink_id_ = DisplaySourceConnectionDelegate::kInvalidSinkId;
  }

  SinkState actual_state = found->state;

  if (actual_state == sink_state_)
    return;

  if (actual_state == SINK_STATE_CONNECTED) {
    auto connection = delegate_->connection();
    DCHECK(connection);
    auto on_message = base::Bind(&WiFiDisplaySessionServiceImpl::OnSinkMessage,
                                 weak_factory_.GetWeakPtr());
    connection->SetMessageReceivedCallback(on_message);
    client_->OnConnected(connection->GetLocalAddress(),
                         connection->GetSinkAddress());
  }

  if (actual_state == SINK_STATE_DISCONNECTED) {
    client_->OnDisconnectRequestHandled(true, "");
    client_->OnTerminated();
    sink_id_ = DisplaySourceConnectionDelegate::kInvalidSinkId;
  }

  sink_state_ = actual_state;
}

void WiFiDisplaySessionServiceImpl::OnConnectionError(
    int sink_id,
    DisplaySourceErrorType type,
    const std::string& description) {
  if (sink_id != sink_id_)
    return;
  DCHECK(client_);
  client_->OnError(type, description);
}

void WiFiDisplaySessionServiceImpl::OnConnectFailed(
    int sink_id,
    const std::string& message) {
  if (sink_id != sink_id_)
    return;
  DCHECK(client_);
  client_->OnError(ERROR_TYPE_CONNECTION_ERROR, message);
}

void WiFiDisplaySessionServiceImpl::OnDisconnectFailed(
    int sink_id,
    const std::string& message) {
  if (sink_id != sink_id_)
    return;
  DCHECK(client_);
  client_->OnDisconnectRequestHandled(false, message);
}

void WiFiDisplaySessionServiceImpl::OnClientConnectionError() {
  DLOG(ERROR) << "IPC connection error";
  receiver_->reset();
}

}  // namespace extensions
