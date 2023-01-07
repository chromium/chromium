// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/test/shell_apitest.h"

#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

ShellApiTest::ShellApiTest() {
}

ShellApiTest::~ShellApiTest() {
}

const Extension* ShellApiTest::LoadExtension(const std::string& extension_dir) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  base::PathService::Get(extensions::DIR_TEST_DATA, &test_data_dir);
  base::FilePath extension_path = test_data_dir.AppendASCII(extension_dir);

  return extension_system_->LoadExtension(extension_path);
}

const Extension* ShellApiTest::LoadExtension(
    const base::FilePath& extension_path) {
  return extension_system_->LoadExtension(extension_path);
}

const Extension* ShellApiTest::LoadApp(const std::string& app_dir) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath test_data_dir;
  base::PathService::Get(extensions::DIR_TEST_DATA, &test_data_dir);
  base::FilePath app_path = test_data_dir.AppendASCII(app_dir);

  const Extension* extension = extension_system_->LoadApp(app_path);
  if (extension)
    extension_system_->LaunchApp(extension->id());
  return extension;
}

bool ShellApiTest::RunExtensionTest(const std::string& extension_dir) {
  ResultCatcher catcher;
  return RunTest(LoadExtension(extension_dir), &catcher);
}

bool ShellApiTest::RunAppTest(const std::string& app_dir) {
  ResultCatcher catcher;
  return RunTest(LoadApp(app_dir), &catcher);
}

bool ShellApiTest::RunTest(const Extension* extension, ResultCatcher* catcher) {
  if (!extension)
    return false;

  if (!catcher->GetNextResult()) {
    message_ = catcher->message();
    return false;
  }

  return true;
}

void ShellApiTest::UnloadApp(const Extension* app) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  ASSERT_TRUE(registry->RemoveEnabled(app->id()));

  UnloadedExtensionReason reason(UnloadedExtensionReason::DISABLE);
  registry->TriggerOnUnloaded(app, reason);
}

}  // namespace extensions
