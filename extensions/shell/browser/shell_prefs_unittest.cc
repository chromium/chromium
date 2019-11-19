// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_prefs.h"

#include "base/macros.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "extensions/common/extension_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

// A BrowserContext that uses a test data directory as its data path.
class PrefsTestBrowserContext : public content::TestBrowserContext {
 public:
  PrefsTestBrowserContext() {}
  ~PrefsTestBrowserContext() override {}

  // content::BrowserContext:
  base::FilePath GetPath() override {
    base::FilePath path;
    base::PathService::Get(extensions::DIR_TEST_DATA, &path);
    return path.AppendASCII("shell_prefs");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefsTestBrowserContext);
};

class ShellPrefsTest : public testing::Test {
 public:
  ShellPrefsTest() {}
  ~ShellPrefsTest() override {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
  PrefsTestBrowserContext browser_context_;
};

TEST_F(ShellPrefsTest, CreateLocalState) {
  std::unique_ptr<PrefService> local_state =
      shell_prefs::CreateLocalState(browser_context_.GetPath());
  ASSERT_TRUE(local_state);

#if defined(OS_CHROMEOS)
  // Verify prefs were registered.
  EXPECT_TRUE(local_state->FindPreference("hardware.audio_output_enabled"));

  // Verify the test values were read.
  EXPECT_FALSE(local_state->GetBoolean("hardware.audio_output_enabled"));
#endif
}

TEST_F(ShellPrefsTest, CreateUserPrefService) {
  // Create the pref service. This loads the test pref file.
  std::unique_ptr<PrefService> service =
      shell_prefs::CreateUserPrefService(&browser_context_);

  // Some basic extension preferences are registered.
  EXPECT_TRUE(service->FindPreference("extensions.settings"));
  EXPECT_TRUE(service->FindPreference("extensions.toolbarsize"));
  EXPECT_FALSE(service->FindPreference("should.not.exist"));

  // User prefs from the file have been read correctly.
  EXPECT_EQ("1.2.3.4", service->GetString("extensions.last_chrome_version"));
  EXPECT_EQ(123, service->GetInteger("extensions.toolbarsize"));

  // The user prefs system has been initialized.
  EXPECT_EQ(service.get(), user_prefs::UserPrefs::Get(&browser_context_));
}

}  // namespace
}  // namespace extensions
