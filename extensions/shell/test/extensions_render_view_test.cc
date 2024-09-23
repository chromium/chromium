// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/extensions_render_view_test.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "extensions/renderer/api/core_extensions_renderer_api_provider.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/common/shell_content_client.h"
#include "extensions/shell/common/shell_extensions_client.h"
#include "extensions/shell/renderer/api/shell_extensions_renderer_api_provider.h"
#include "extensions/shell/renderer/shell_content_renderer_client.h"
#include "extensions/shell/renderer/shell_extensions_renderer_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {

ExtensionsRenderViewTest::ExtensionsRenderViewTest() = default;
ExtensionsRenderViewTest::~ExtensionsRenderViewTest() = default;

void ExtensionsRenderViewTest::SetUp() {
  base::FilePath extensions_shell_and_test_pak_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS,
                               &extensions_shell_and_test_pak_path));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      extensions_shell_and_test_pak_path.AppendASCII(
          "extensions_shell_and_test.pak"));

  content::RenderViewTest::SetUp();
}

void ExtensionsRenderViewTest::TearDown() {
  content::RenderViewTest::TearDown();

  ui::ResourceBundle::CleanupSharedInstance();
}

content::ContentClient* ExtensionsRenderViewTest::CreateContentClient() {
  return new ShellContentClient();
}

content::ContentBrowserClient*
ExtensionsRenderViewTest::CreateContentBrowserClient() {
  // Our base class does not create a BrowserMainParts, so we don't create the
  // delegate.
  return new ShellContentBrowserClient(/*browser_main_delegate=*/nullptr);
}

content::ContentRendererClient*
ExtensionsRenderViewTest::CreateContentRendererClient() {
  ShellContentRendererClient* client = new ShellContentRendererClient();

  auto extensions_client = std::make_unique<ShellExtensionsRendererClient>();
  extensions_client->AddAPIProvider(
      std::make_unique<CoreExtensionsRendererAPIProvider>());
  extensions_client->AddAPIProvider(
      std::make_unique<ShellExtensionsRendererAPIProvider>());
  extensions_client->RenderThreadStarted();

  // Note that creation order is important here. The Dispatcher needs to be
  // created after our base class creates the fake RenderThread, but before it
  // creates the test frame. Since `client` would not have observed
  // RenderThreadStarted, we create the extensions clients here.
  client->SetClientsForTesting(std::make_unique<ShellExtensionsClient>(),
                               std::move(extensions_client));

  return client;
}

}  // namespace extensions
