// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_RENDERER_SHELL_EXTENSIONS_RENDERER_CLIENT_H_
#define EXTENSIONS_SHELL_RENDERER_SHELL_EXTENSIONS_RENDERER_CLIENT_H_

#include "extensions/renderer/extensions_renderer_client.h"

namespace extensions {

class ShellExtensionsRendererClient : public ExtensionsRendererClient {
 public:
  ShellExtensionsRendererClient();
  ShellExtensionsRendererClient(const ShellExtensionsRendererClient&) = delete;
  ShellExtensionsRendererClient& operator=(
      const ShellExtensionsRendererClient&) = delete;
  ~ShellExtensionsRendererClient() override;

  // ExtensionsRendererClient implementation.
  bool IsIncognitoProcess() const override;
  int GetLowestIsolatedWorldId() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_RENDERER_SHELL_EXTENSIONS_RENDERER_CLIENT_H_
