// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_TEST_SHELL_APITEST_H_
#define EXTENSIONS_SHELL_TEST_SHELL_APITEST_H_

#include <string>

#include "base/files/file_path.h"
#include "extensions/shell/test/shell_test.h"

namespace extensions {

class Extension;
class ResultCatcher;

// Base class for app shell browser API tests that load an app/extension
// and wait for a success message from the chrome.test API.
class ShellApiTest : public AppShellTest {
 public:
  ShellApiTest();

  ShellApiTest(const ShellApiTest&) = delete;
  ShellApiTest& operator=(const ShellApiTest&) = delete;

  ~ShellApiTest() override;

  // Loads an unpacked extension. Returns an instance of the extension that was
  // just loaded.
  // |extension_dir| should be a subpath under extensions/test/data.
  const Extension* LoadExtension(const std::string& extension_dir);

  // Loads an unpacked extension. Returns an instance of the extension that was
  // just loaded.
  // |extension_path| should be an absolute path to the extension.
  const Extension* LoadExtension(const base::FilePath& extension_path);

  // Loads and launches an unpacked platform app. Returns an instance of the
  // extension that was just loaded.
  // |app_dir| should be a subpath under extensions/test/data.
  const Extension* LoadApp(const std::string& app_dir);

  // Loads an unpacked extension and waits for a chrome.test success
  // notification. Returns true if the test succeeds.
  bool RunExtensionTest(const std::string& extension_dir);

  // Loads and launches an unpacked platform app and waits for a chrome.test
  // success notification. Returns true if the test succeeds.
  bool RunAppTest(const std::string& app_dir);

  // Removes the |app| from the ExtensionRegistry and dispatches
  // notifications of the removal stating reason as REASON_DISABLE.
  void UnloadApp(const Extension* app);

 protected:
  // If it failed, what was the error message?
  std::string message_;

 private:
  bool RunTest(const Extension* extension, ResultCatcher* catcher);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_TEST_SHELL_APITEST_H_
