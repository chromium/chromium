// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_test_extensions_browser_client.h"

#include "build/build_config.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"

namespace extensions {

ShellTestExtensionsBrowserClient::ShellTestExtensionsBrowserClient(
    content::BrowserContext* main_context)
    : TestExtensionsBrowserClient(main_context) {}

ShellTestExtensionsBrowserClient::ShellTestExtensionsBrowserClient() = default;

ShellTestExtensionsBrowserClient::~ShellTestExtensionsBrowserClient() = default;

ExtensionWebContentsObserver*
ShellTestExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return ShellExtensionWebContentsObserver::FromWebContents(web_contents);
}

}  // namespace extensions
