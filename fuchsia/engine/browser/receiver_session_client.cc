// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/receiver_session_client.h"

#include "base/bind.h"
#include "components/cast/message_port/message_port_fuchsia.h"

ReceiverSessionClient::ReceiverSessionClient(
    fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request)
    : message_port_request_(std::move(message_port_request)) {
  DCHECK(message_port_request_);
}

ReceiverSessionClient::~ReceiverSessionClient() = default;

void ReceiverSessionClient::SetCastStreamingReceiver(
    mojo::AssociatedRemote<mojom::CastStreamingReceiver>
        cast_streaming_receiver) {
  DCHECK(message_port_request_);

  receiver_session_ = cast_streaming::ReceiverSession::Create(base::BindOnce(
      [](fidl::InterfaceRequest<fuchsia::web::MessagePort> port)
          -> std::unique_ptr<cast_api_bindings::MessagePort> {
        return cast_api_bindings::MessagePortFuchsia::Create(std::move(port));
      },
      std::move(message_port_request_)));
  receiver_session_->SetCastStreamingReceiver(
      std::move(cast_streaming_receiver));
}
