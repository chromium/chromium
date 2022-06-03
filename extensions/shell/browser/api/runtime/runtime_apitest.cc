// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

using ShellRuntimeApiTest = ShellApiTest;

IN_PROC_BROWSER_TEST_F(ShellRuntimeApiTest, RuntimeReload) {
  scoped_refptr<const Extension> extension;

  // Load the extension and wait for it to be ready.
  {
    ResultCatcher catcher;
    ASSERT_TRUE(extension = base::WrapRefCounted(LoadExtension("extension")));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  const ExtensionId extension_id = extension->id();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  // Reload the extension and wait for a pair of
  // ExtensionRegistry::OnExtensionUnloaded()/Loaded() calls.
  TestExtensionRegistryObserver registry_observer(registry, extension_id);
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser_context(), extension_id, "chrome.runtime.reload();"));
  ASSERT_EQ(extension, registry_observer.WaitForExtensionUnloaded());
  EXPECT_TRUE(registry->disabled_extensions().Contains(extension_id));
  ASSERT_TRUE(extension = registry_observer.WaitForExtensionLoaded());
  ASSERT_EQ(extension->id(), extension_id);
  EXPECT_TRUE(registry->enabled_extensions().Contains(extension_id));

  // Wait for the background page to load.
  {
    ResultCatcher catcher;
    ASSERT_TRUE(catcher.GetNextResult());
  }
}

IN_PROC_BROWSER_TEST_F(ShellRuntimeApiTest, RuntimeReloadApp) {
  scoped_refptr<const Extension> extension;

  // Load and launch the app and wait for it to create a window.
  {
    ResultCatcher catcher;
    extension = base::WrapRefCounted(LoadApp("platform_app"));
    ASSERT_TRUE(catcher.GetNextResult());
  }

  const ExtensionId extension_id = extension->id();
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  // Reload the extension and wait for a pair of
  // ExtensionRegistry::OnExtensionUnloaded()/Loaded() calls.
  TestExtensionRegistryObserver registry_observer(registry, extension_id);
  ASSERT_TRUE(browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      browser_context(), extension_id, "chrome.runtime.reload();"));
  ASSERT_EQ(extension, registry_observer.WaitForExtensionUnloaded());
  EXPECT_TRUE(registry->disabled_extensions().Contains(extension_id));
  ASSERT_TRUE(extension = registry_observer.WaitForExtensionLoaded());
  ASSERT_EQ(extension->id(), extension_id);
  EXPECT_TRUE(registry->enabled_extensions().Contains(extension_id));

  // Reloading the app should launch it again automatically.
  // Wait for the app to create a new window.
  {
    ResultCatcher catcher;
    ASSERT_TRUE(catcher.GetNextResult());
  }
}

}  // namespace extensions
