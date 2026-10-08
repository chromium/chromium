// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/no_op_mdns_responder.h"

#include <memory>
#include <string>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

// static
void NoOpMdnsResponder::Create(
    mojo::PendingReceiver<network::mojom::MdnsResponder> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<NoOpMdnsResponder>(),
                              std::move(receiver));
}

NoOpMdnsResponder::NoOpMdnsResponder() = default;
NoOpMdnsResponder::~NoOpMdnsResponder() = default;

void NoOpMdnsResponder::CreateNameForAddress(
    const net::IPAddress& address,
    CreateNameForAddressCallback callback) {
  std::move(callback).Run(/*name=*/std::string(),
                          /*announcement_scheduled=*/false);
}

void NoOpMdnsResponder::RemoveNameForAddress(
    const net::IPAddress& address,
    RemoveNameForAddressCallback callback) {
  std::move(callback).Run(/*removed=*/false, /*goodbye_scheduled=*/false);
}

}  // namespace content
