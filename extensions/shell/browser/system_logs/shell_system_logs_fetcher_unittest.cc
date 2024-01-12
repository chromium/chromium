// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/system_logs/shell_system_logs_fetcher.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"

namespace extensions {

class ShellSystemLogsFetcherTest : public ExtensionsTest {
 public:
  ShellSystemLogsFetcherTest() = default;
  ~ShellSystemLogsFetcherTest() override = default;

  scoped_refptr<const Extension> BuildExtension(const std::string& name,
                                                const std::string& version,
                                                const std::string& id) {
    return ExtensionBuilder(name).SetVersion(version).SetID(id).Build();
  }

  void OnSystemLogsResponse(
      std::unique_ptr<system_logs::SystemLogsResponse> response) {
    response_ = std::move(response);
    wait_for_logs_response_run_loop_.Quit();
  }

  const system_logs::SystemLogsResponse* response() const {
    return response_.get();
  }

 protected:
  base::RunLoop wait_for_logs_response_run_loop_;

 private:
  std::unique_ptr<system_logs::SystemLogsResponse> response_;
};

// Tests that basic log source includes version tags and extensions.
TEST_F(ShellSystemLogsFetcherTest, TestLogSources) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  EXPECT_TRUE(registry);

  std::vector<scoped_refptr<const Extension>> extensions{
      BuildExtension("My First Extension", "1.1", std::string(32, 'g')),
      BuildExtension("My Second Extension", "1.2", std::string(32, 'h'))};
  for (const scoped_refptr<const Extension>& extension : extensions)
    registry->AddEnabled(extension);

  system_logs::SystemLogsFetcher* fetcher =
      system_logs::BuildShellSystemLogsFetcher(browser_context());
  fetcher->Fetch(
      base::BindOnce(&ShellSystemLogsFetcherTest::OnSystemLogsResponse,
                     base::Unretained(this)));

  wait_for_logs_response_run_loop_.Run();

  ASSERT_TRUE(response());
  EXPECT_LT(0u, response()->at("APPSHELL VERSION").size());
  EXPECT_LT(0u, response()->at("OS VERSION").size());

  const std::string_view fmt = "$1 : $2 : version $3\n";
  std::string expected_extensions = "";
  for (const scoped_refptr<const Extension>& extension : extensions) {
    std::string version_mangled;
    base::ReplaceChars(extension->VersionString(), ".", "_", &version_mangled);
    expected_extensions += base::ReplaceStringPlaceholders(
        fmt, {extension->id(), extension->name(), version_mangled}, nullptr);
  }
  EXPECT_EQ(expected_extensions, response()->at("extensions"));

  for (const scoped_refptr<const Extension>& extension : extensions)
    registry->RemoveEnabled(extension->id());
}

}  // namespace extensions
