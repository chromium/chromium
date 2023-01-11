// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_socket_mojo_remote.h"

#include "third_party/blink/public/mojom/direct_sockets/direct_sockets.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

UDPSocketMojoRemote::UDPSocketMojoRemote(ExecutionContext* execution_context)
    : udp_socket_{execution_context} {}

UDPSocketMojoRemote::~UDPSocketMojoRemote() = default;

void UDPSocketMojoRemote::Close() {
  udp_socket_.reset();
}

void UDPSocketMojoRemote::Trace(Visitor* visitor) const {
  visitor->Trace(udp_socket_);
}

}  // namespace blink
