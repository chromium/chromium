// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_TEST_EXTENSIONS_RENDERER_CLIENT_H_
#define EXTENSIONS_RENDERER_TEST_EXTENSIONS_RENDERER_CLIENT_H_

#include "extensions/renderer/extensions_renderer_client.h"

namespace extensions {

class TestExtensionsRendererClient : public ExtensionsRendererClient {
 public:
  TestExtensionsRendererClient();
  TestExtensionsRendererClient(const TestExtensionsRendererClient&) = delete;
  TestExtensionsRendererClient& operator=(const TestExtensionsRendererClient&) =
      delete;
  ~TestExtensionsRendererClient() override;

  // ExtensionsRendererClient implementation.
  bool IsIncognitoProcess() const override;
  int GetLowestIsolatedWorldId() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_TEST_EXTENSIONS_RENDERER_CLIENT_H_
