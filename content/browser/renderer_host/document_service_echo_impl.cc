// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/document_service_echo_impl.h"

namespace content {

DocumentServiceEchoImpl::DocumentServiceEchoImpl(
    RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::Echo> receiver,
    base::OnceClosure destruction_cb)
    : DocumentService(render_frame_host, std::move(receiver)),
      destruction_cb_(std::move(destruction_cb)) {}

DocumentServiceEchoImpl::~DocumentServiceEchoImpl() {
  std::move(destruction_cb_).Run();
}

void DocumentServiceEchoImpl::EchoString(const std::string& input,
                                         EchoStringCallback callback) {
  std::move(callback).Run(input);
}

}  // namespace content
