// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/file_handlers/directory_util.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace app_file_handler_util {
namespace {

const char kRandomPath[] = "/random/path";

void OnCollectForEntriesPath(
    std::set<base::FilePath>* output,
    std::unique_ptr<std::set<base::FilePath>> path_directory_set) {
  *output = *path_directory_set;
}

}  // namespace

class IsDirectoryUtilTest : public testing::Test {
 protected:
  IsDirectoryUtilTest() {}
  ~IsDirectoryUtilTest() override {}

  void SetUp() override {
    EXPECT_TRUE(
        base::CreateNewTempDirectory(base::FilePath::StringType(), &dir_path_));
    EXPECT_TRUE(base::CreateTemporaryFile(&file_path_));
  }

  content::BrowserTaskEnvironment task_environment_;
  ExtensionsAPIClient extensions_api_client_;
  content::TestBrowserContext context_;
  base::FilePath dir_path_;
  base::FilePath file_path_;
};

TEST_F(IsDirectoryUtilTest, CollectForEntriesPaths) {
  std::vector<base::FilePath> paths;
  paths.push_back(dir_path_);
  paths.push_back(file_path_);
  paths.push_back(base::FilePath::FromUTF8Unsafe(kRandomPath));

  IsDirectoryCollector collector(&context_);
  std::set<base::FilePath> result;
  collector.CollectForEntriesPaths(
      paths, base::BindOnce(&OnCollectForEntriesPath, &result));
  content::RunAllTasksUntilIdle();

  ASSERT_EQ(1u, result.size());
  EXPECT_GT(result.count(dir_path_), 0u);
}

}  // namespace app_file_handler_util
}  // namespace extensions
