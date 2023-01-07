// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_frame_host.h"

#include <string>

#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_web_contents_observer.h"

namespace extensions {

ExtensionFrameHost::ExtensionFrameHost(content::WebContents* web_contents)
    : web_contents_(web_contents), receivers_(web_contents, this) {}

ExtensionFrameHost::~ExtensionFrameHost() = default;

void ExtensionFrameHost::BindLocalFrameHost(
    mojo::PendingAssociatedReceiver<mojom::LocalFrameHost> receiver,
    content::RenderFrameHost* rfh) {
  receivers_.Bind(rfh, std::move(receiver));
}

void ExtensionFrameHost::RequestScriptInjectionPermission(
    const std::string& extension_id,
    mojom::InjectionType script_type,
    mojom::RunLocation run_location,
    RequestScriptInjectionPermissionCallback callback) {
  std::move(callback).Run(false);
}

void ExtensionFrameHost::GetAppInstallState(
    const GURL& requestor_url,
    GetAppInstallStateCallback callback) {
  std::move(callback).Run(std::string());
}

void ExtensionFrameHost::Request(mojom::RequestParamsPtr params,
                                 RequestCallback callback) {
  content::RenderFrameHost* render_frame_host =
      receivers_.GetCurrentTargetFrame();
  ExtensionWebContentsObserver::GetForWebContents(web_contents_)
      ->dispatcher()
      ->Dispatch(std::move(params), *render_frame_host, std::move(callback));
}

void ExtensionFrameHost::WatchedPageChange(
    const std::vector<std::string>& css_selectors) {}

void ExtensionFrameHost::DetailedConsoleMessageAdded(
    const std::u16string& message,
    const std::u16string& source,
    const StackTrace& stack_trace,
    blink::mojom::ConsoleMessageLevel level) {}

}  // namespace extensions
