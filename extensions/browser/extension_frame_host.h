// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FRAME_HOST_H_
#define EXTENSIONS_BROWSER_EXTENSION_FRAME_HOST_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"

namespace content {
class WebContents;
}

namespace extensions {

// Implements the mojo interface of extensions::mojom::LocalFrameHost.
// ExtensionWebContentsObserver creates and owns this class and it's destroyed
// when WebContents is destroyed.
class ExtensionFrameHost : public mojom::LocalFrameHost {
 public:
  explicit ExtensionFrameHost(content::WebContents* web_contents);
  ExtensionFrameHost(const ExtensionFrameHost&) = delete;
  ExtensionFrameHost& operator=(const ExtensionFrameHost&) = delete;
  ~ExtensionFrameHost() override;

  void BindLocalFrameHost(
      mojo::PendingAssociatedReceiver<mojom::LocalFrameHost> receiver,
      content::RenderFrameHost* rfh);

  content::RenderFrameHostReceiverSet<mojom::LocalFrameHost>&
  receivers_for_testing() {
    return receivers_;
  }

  // mojom::LocalFrameHost:
  void RequestScriptInjectionPermission(
      const std::string& extension_id,
      mojom::InjectionType script_type,
      mojom::RunLocation run_location,
      RequestScriptInjectionPermissionCallback callback) override;
  void GetAppInstallState(const GURL& requestor_url,
                          GetAppInstallStateCallback callback) override;
  void Request(mojom::RequestParamsPtr params,
               RequestCallback callback) override;
  void WatchedPageChange(
      const std::vector<std::string>& css_selectors) override;
  void DetailedConsoleMessageAdded(
      const std::u16string& message,
      const std::u16string& source,
      const StackTrace& stack_trace,
      blink::mojom::ConsoleMessageLevel level) override;

 protected:
  // This raw pointer is safe to use because ExtensionWebContentsObserver whose
  // lifetime is tied to the WebContents owns this instance.
  raw_ptr<content::WebContents> web_contents_;
  content::RenderFrameHostReceiverSet<mojom::LocalFrameHost> receivers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_FRAME_HOST_H_
