// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_EXTENSIONS_RENDER_VIEW_TEST_H_
#define EXTENSIONS_SHELL_TEST_EXTENSIONS_RENDER_VIEW_TEST_H_

#include "content/public/test/render_view_test.h"

namespace extensions {

// Provides the app_shell implementations of content embedder APIs
// for RenderViewTests.
class ExtensionsRenderViewTest : public content::RenderViewTest {
 public:
  ExtensionsRenderViewTest();
  ~ExtensionsRenderViewTest() override;

 private:
  // content::RenderViewTest:
  void SetUp() override;
  void TearDown() override;
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_EXTENSIONS_RENDER_VIEW_TEST_H_
