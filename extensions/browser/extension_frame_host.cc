// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_frame_host.h"

namespace extensions {

ExtensionFrameHost::ExtensionFrameHost(content::WebContents* web_contents)
    : receivers_(web_contents, this) {}

ExtensionFrameHost::~ExtensionFrameHost() = default;

void ExtensionFrameHost::RequestScriptInjectionPermission(
    const std::string& extension_id,
    mojom::InjectionType script_type,
    mojom::RunLocation run_location,
    RequestScriptInjectionPermissionCallback callback) {
  std::move(callback).Run(false);
}

}  // namespace extensions
