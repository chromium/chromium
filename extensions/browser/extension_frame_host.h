// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FRAME_HOST_H_
#define EXTENSIONS_BROWSER_EXTENSION_FRAME_HOST_H_

#include "content/public/browser/web_contents_receiver_set.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"

namespace content {
class WebContents;
}

namespace extensions {

// Implements the mojo interface of extensions::mojom::LocalFrameHost.
// ExtensionWebContentsObserver creates this class and it's destroyed with it
// when WebContents is destroyed.
class ExtensionFrameHost : public mojom::LocalFrameHost {
 public:
  explicit ExtensionFrameHost(content::WebContents* web_contents);
  ExtensionFrameHost(const ExtensionFrameHost&) = delete;
  ExtensionFrameHost& operator=(const ExtensionFrameHost&) = delete;
  ~ExtensionFrameHost() override;

  // mojom::LocalFrameHost:
  void RequestScriptInjectionPermission(
      const std::string& extension_id,
      mojom::InjectionType script_type,
      mojom::RunLocation run_location,
      RequestScriptInjectionPermissionCallback callback) override;

 private:
  content::WebContentsFrameReceiverSet<mojom::LocalFrameHost> receivers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_FRAME_HOST_H_
