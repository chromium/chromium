// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/remote_open_url/remote_open_url_message_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_serialization.h"
#include "url/gurl.h"

namespace remoting {

namespace {

using OpenUrlResponse = protocol::RemoteOpenUrl::OpenUrlResponse;

mojom::OpenUrlResult ToMojomOpenUrlResult(
    OpenUrlResponse::Result protobuf_result) {
  switch (protobuf_result) {
    case OpenUrlResponse::UNSPECIFIED_OPEN_URL_RESULT:
      return mojom::OpenUrlResult::UNSPECIFIED_OPEN_URL_RESULT;
    case OpenUrlResponse::SUCCESS:
      return mojom::OpenUrlResult::SUCCESS;
    case OpenUrlResponse::FAILURE:
      return mojom::OpenUrlResult::FAILURE;
    case OpenUrlResponse::LOCAL_FALLBACK:
      return mojom::OpenUrlResult::LOCAL_FALLBACK;
  }
}

}  // namespace

RemoteOpenUrlMessageHandler::RemoteOpenUrlMessageHandler(
    const std::string& name,
    std::unique_ptr<protocol::MessagePipe> pipe)
    : protocol::NamedMessagePipeHandler(name, std::move(pipe)) {
  receivers_.set_disconnect_handler(base::BindRepeating(
      &RemoteOpenUrlMessageHandler::OnIpcDisconnected, base::Unretained(this)));
}

RemoteOpenUrlMessageHandler::~RemoteOpenUrlMessageHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnDisconnecting();
}

void RemoteOpenUrlMessageHandler::AddReceiver(
    mojo::PendingReceiver<mojom::RemoteUrlOpener> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AddReceiverAndGetReceiverId(std::move(receiver));
}

void RemoteOpenUrlMessageHandler::ClearReceivers() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Clear();
}

void RemoteOpenUrlMessageHandler::OnIncomingMessage(
    std::unique_ptr<CompoundBuffer> message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto remote_open_url =
      protocol::ParseMessage<protocol::RemoteOpenUrl>(message.get());
  if (!remote_open_url->has_open_url_response()) {
    LOG(WARNING)
        << "Received a RemoteOpenUrl message without open_url_response.";
    return;
  }
  const auto& response = remote_open_url->open_url_response();
  auto it = callbacks_.find(response.id());
  if (it == callbacks_.end()) {
    LOG(WARNING) << "Untracked remote open URL ID: " << response.id();
    return;
  }
  std::move(it->second).Run(ToMojomOpenUrlResult(response.result()));
  callbacks_.erase(it);
  receivers_.Remove(response.id());
}

void RemoteOpenUrlMessageHandler::OnDisconnecting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The remote connection is going away, so inform all IPC clients to open the
  // URL locally.
  for (auto& entry : callbacks_) {
    std::move(entry.second).Run(mojom::OpenUrlResult::LOCAL_FALLBACK);
  }
  callbacks_.clear();
  receivers_.Clear();
}

base::WeakPtr<RemoteOpenUrlMessageHandler>
RemoteOpenUrlMessageHandler::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

mojo::ReceiverId RemoteOpenUrlMessageHandler::AddReceiverAndGetReceiverId(
    mojo::PendingReceiver<mojom::RemoteUrlOpener> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!connected()) {
    LOG(WARNING) << "RemoteOpenUrlMessageHandler has not been connected";
    return 0u;
  }
  return receivers_.Add(this, std::move(receiver));
}

void RemoteOpenUrlMessageHandler::OnIpcDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = callbacks_.find(receivers_.current_receiver());
  if (it != callbacks_.end()) {
    LOG(WARNING)
        << "The client has disconnected before the response is received.";
    callbacks_.erase(it);
  }
}

void RemoteOpenUrlMessageHandler::OpenUrl(const GURL& url,
                                          OpenUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!url.is_valid()) {
    std::move(callback).Run(mojom::OpenUrlResult::FAILURE);
    receivers_.Remove(receivers_.current_receiver());
    return;
  }

  mojo::ReceiverId id = receivers_.current_receiver();
  DCHECK(callbacks_.find(id) == callbacks_.end())
      << "The client has made more than one call to OpenUrl().";

  protocol::RemoteOpenUrl remote_open_url;
  remote_open_url.mutable_open_url_request()->set_id(id);
  remote_open_url.mutable_open_url_request()->set_url(url.spec());
  Send(remote_open_url, base::DoNothing());
  callbacks_[id] = std::move(callback);
}

}  // namespace remoting
