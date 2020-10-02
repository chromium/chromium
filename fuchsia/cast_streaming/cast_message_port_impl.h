// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_CAST_MESSAGE_PORT_IMPL_H_
#define FUCHSIA_CAST_STREAMING_CAST_MESSAGE_PORT_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include "base/containers/circular_deque.h"
#include "third_party/openscreen/src/cast/common/public/message_port.h"

namespace cast_streaming {

// Wrapper for a fuchsia.web.MessagePort that provides an Open Screen
// MessagePort implementation.
class CastMessagePortImpl : public openscreen::cast::MessagePort,
                            public fuchsia::web::MessagePort {
 public:
  explicit CastMessagePortImpl(
      fidl::InterfaceRequest<fuchsia::web::MessagePort> message_port_request);
  ~CastMessagePortImpl() final;

  CastMessagePortImpl(const CastMessagePortImpl&) = delete;
  CastMessagePortImpl& operator=(const CastMessagePortImpl&) = delete;

  // openscreen::cast::MessagePort implementation.
  void SetClient(Client* client, std::string client_sender_id) final;
  void ResetClient() final;
  void PostMessage(const std::string& sender_id,
                   const std::string& message_namespace,
                   const std::string& message) final;

 private:
  // Sends one message in |pending_fidl_messages_| if
  // |receive_message_callback_| is set.
  void MaybeSendMessageToFidl();

  // Closes the fuchsia.web.MessagePort connection and cleans up internal state.
  // * Closes |message_port_binding_| with |epitaph| if it is still open.
  // * Signals an error to |client_| if |client_| is set.
  // * Empties |pending_fidl_messages_|.
  void MaybeCloseWithEpitaph(zx_status_t epitaph);

  // Returns a "not supported" error message to the sender for messages from
  // the inject namespace.
  void SendInjectResponse(const std::string& sender_id,
                          const std::string& message);

  // fuchsia::web::MessagePort implementation.
  void PostMessage(fuchsia::web::WebMessage message,
                   PostMessageCallback callback) final;
  void ReceiveMessage(ReceiveMessageCallback callback) final;

  Client* client_ = nullptr;

  // Holds WebMessages waiting to be sent over FIDL.
  base::circular_deque<fuchsia::web::WebMessage> pending_fidl_messages_;

  ReceiveMessageCallback receive_message_callback_;

  fidl::Binding<fuchsia::web::MessagePort> message_port_binding_;
};

}  // namespace cast_streaming

#endif  // FUCHSIA_CAST_STREAMING_CAST_MESSAGE_PORT_IMPL_H_
