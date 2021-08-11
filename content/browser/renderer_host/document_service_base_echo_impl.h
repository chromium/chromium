// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DOCUMENT_SERVICE_BASE_ECHO_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_DOCUMENT_SERVICE_BASE_ECHO_IMPL_H_

#include "base/bind.h"
#include "content/public/browser/document_service_base.h"
#include "content/test/echo.test-mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class RenderFrameHost;

// Subclass of DocumentServiceBase for test.
class DocumentServiceBaseEchoImpl final
    : public DocumentServiceBase<mojom::Echo> {
 public:
  DocumentServiceBaseEchoImpl(RenderFrameHost* render_frame_host,
                              mojo::PendingReceiver<mojom::Echo> receiver,
                              base::OnceClosure destruction_cb);
  ~DocumentServiceBaseEchoImpl() final;

  // mojom::Echo implementation
  void EchoString(const std::string& input, EchoStringCallback callback) final;

 private:
  base::OnceClosure destruction_cb_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DOCUMENT_SERVICE_BASE_ECHO_IMPL_H_
