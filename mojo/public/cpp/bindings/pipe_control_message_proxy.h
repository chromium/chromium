// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_PROXY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_PROXY_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/async_flusher.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_flush.h"

namespace mojo {

class MessageReceiver;

// Proxy for request messages defined in pipe_control_messages.mojom.
//
// NOTE: This object may be used from multiple sequences.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) PipeControlMessageProxy {
 public:
  // Doesn't take ownership of |receiver|. If This PipeControlMessageProxy will
  // be used from multiple sequences, |receiver| must be thread-safe.
  explicit PipeControlMessageProxy(MessageReceiver* receiver);

  PipeControlMessageProxy(const PipeControlMessageProxy&) = delete;
  PipeControlMessageProxy& operator=(const PipeControlMessageProxy&) = delete;

  void NotifyPeerEndpointClosed(InterfaceId id,
                                const std::optional<DisconnectReason>& reason);
  void PausePeerUntilFlushCompletes(PendingFlush flush);
  void FlushAsync(AsyncFlusher flusher);

  static Message ConstructPeerEndpointClosedMessage(
      InterfaceId id,
      const std::optional<DisconnectReason>& reason);

 private:
  // Not owned.
  raw_ptr<MessageReceiver> receiver_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_PROXY_H_
