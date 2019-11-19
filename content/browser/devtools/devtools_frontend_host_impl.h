// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_IMPL_H_

#include "base/macros.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/mojom/devtools/devtools_frontend.mojom.h"

namespace content {

class WebContents;

class DevToolsFrontendHostImpl : public DevToolsFrontendHost,
                                 public blink::mojom::DevToolsFrontendHost {
 public:
  DevToolsFrontendHostImpl(
      RenderFrameHost* frame_host,
      const HandleMessageCallback& handle_message_callback);
  ~DevToolsFrontendHostImpl() override;

  void BadMessageRecieved() override;

 private:
  // blink::mojom::DevToolsFrontendHost implementation.
  void DispatchEmbedderMessage(const std::string& message) override;

  WebContents* web_contents_;
  HandleMessageCallback handle_message_callback_;
  mojo::AssociatedReceiver<blink::mojom::DevToolsFrontendHost> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(DevToolsFrontendHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_IMPL_H_
