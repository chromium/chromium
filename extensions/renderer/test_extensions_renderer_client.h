// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_TEST_EXTENSIONS_RENDERER_CLIENT_H_
#define EXTENSIONS_RENDERER_TEST_EXTENSIONS_RENDERER_CLIENT_H_

#include "base/macros.h"
#include "extensions/renderer/extensions_renderer_client.h"

namespace extensions {

class TestExtensionsRendererClient : public ExtensionsRendererClient {
 public:
  TestExtensionsRendererClient();
  ~TestExtensionsRendererClient() override;

  // ExtensionsRendererClient implementation.
  bool IsIncognitoProcess() const override;
  int GetLowestIsolatedWorldId() const override;
  Dispatcher* GetDispatcher() override;
  bool ExtensionAPIEnabledForServiceWorkerScript(
      const GURL& scope,
      const GURL& script_url) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestExtensionsRendererClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_TEST_EXTENSIONS_RENDERER_CLIENT_H_
