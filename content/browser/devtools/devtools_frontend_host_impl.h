// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_IMPL_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "third_party/blink/public/mojom/devtools/devtools_frontend.mojom.h"

namespace content {

class WebContents;

class DevToolsFrontendHostImpl : public DevToolsFrontendHost,
                                 public blink::mojom::DevToolsFrontendHost,
                                 public WebContentsObserver {
 public:
  DevToolsFrontendHostImpl(
      RenderFrameHost* frame_host,
      const HandleMessageCallback& handle_message_callback);

  DevToolsFrontendHostImpl(const DevToolsFrontendHostImpl&) = delete;
  DevToolsFrontendHostImpl& operator=(const DevToolsFrontendHostImpl&) = delete;

  ~DevToolsFrontendHostImpl() override;

  static CONTENT_EXPORT std::unique_ptr<DevToolsFrontendHostImpl>
  CreateForTesting(RenderFrameHost* frame_host,
                   const HandleMessageCallback& handle_message_callback);

  void BadMessageReceived() override;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  void OnDidAddMessageToConsole(
      RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

 private:
  // blink::mojom::DevToolsFrontendHost implementation.
  void DispatchEmbedderMessage(base::Value::Dict message) override;

  raw_ptr<WebContents> web_contents_;
  HandleMessageCallback handle_message_callback_;
  mojo::AssociatedReceiver<blink::mojom::DevToolsFrontendHost> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_FRONTEND_HOST_IMPL_H_
