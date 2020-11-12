// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_CAST_STREAMING_CAST_MESSAGE_PORT_IMPL_H_
#define FUCHSIA_CAST_STREAMING_CAST_MESSAGE_PORT_IMPL_H_

#include "components/cast/message_port/message_port.h"
#include "third_party/openscreen/src/cast/common/public/message_port.h"

namespace cast_streaming {

// Wrapper for a cast MessagePort that provides an Open Screen MessagePort
// implementation.
class CastMessagePortImpl : public openscreen::cast::MessagePort,
                            public cast_api_bindings::MessagePort::Receiver {
 public:
  explicit CastMessagePortImpl(
      std::unique_ptr<cast_api_bindings::MessagePort> message_port);
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
  // Resets |message_port_| if it is open and signals an error to |client_| if
  // |client_| is set.
  void MaybeClose();

  // Returns a "not supported" error message to the sender for messages from
  // the inject namespace.
  void SendInjectResponse(const std::string& sender_id,
                          const std::string& message);

  // cast_api_bindings::MessagePort::Receiver implementation.
  bool OnMessage(
      base::StringPiece message,
      std::vector<std::unique_ptr<cast_api_bindings::MessagePort>> ports) final;
  void OnPipeError() final;

  Client* client_ = nullptr;
  std::unique_ptr<cast_api_bindings::MessagePort> message_port_;
};

}  // namespace cast_streaming

#endif  // FUCHSIA_CAST_STREAMING_CAST_MESSAGE_PORT_IMPL_H_
