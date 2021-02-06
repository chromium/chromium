// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_PROXY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_PROXY_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/cpp/bindings/async_flusher.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"
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

  void NotifyPeerEndpointClosed(InterfaceId id,
                                const base::Optional<DisconnectReason>& reason);
  void PausePeerUntilFlushCompletes(PendingFlush flush);
  void FlushAsync(AsyncFlusher flusher);

  static Message ConstructPeerEndpointClosedMessage(
      InterfaceId id,
      const base::Optional<DisconnectReason>& reason);

 private:
  // Not owned.
  MessageReceiver* receiver_;

  DISALLOW_COPY_AND_ASSIGN(PipeControlMessageProxy);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PIPE_CONTROL_MESSAGE_PROXY_H_
