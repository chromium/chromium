// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NO_OP_MDNS_RESPONDER_H_
#define CONTENT_BROWSER_RENDERER_HOST_NO_OP_MDNS_RESPONDER_H_

#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/mdns_responder.mojom.h"

namespace content {

// An MdnsResponder that never registers or announces any names. It is bound in
// place of the real (network service backed) MdnsResponder for documents whose
// Connection Allowlist blocks WebRTC. All reply callbacks are run with failure
// values so that no renderer request hangs.
class CONTENT_EXPORT NoOpMdnsResponder : public network::mojom::MdnsResponder {
 public:
  // Creates a self-owned NoOpMdnsResponder bound to `receiver`. It is destroyed
  // when the renderer closes the pipe.
  static void Create(
      mojo::PendingReceiver<network::mojom::MdnsResponder> receiver);

  NoOpMdnsResponder();
  NoOpMdnsResponder(const NoOpMdnsResponder&) = delete;
  NoOpMdnsResponder& operator=(const NoOpMdnsResponder&) = delete;
  ~NoOpMdnsResponder() override;

  // network::mojom::MdnsResponder:
  void CreateNameForAddress(const net::IPAddress& address,
                            CreateNameForAddressCallback callback) override;
  void RemoveNameForAddress(const net::IPAddress& address,
                            RemoveNameForAddressCallback callback) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NO_OP_MDNS_RESPONDER_H_
