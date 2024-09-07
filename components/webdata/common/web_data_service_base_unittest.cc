// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_data_service_base.h"

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestTable : public WebDatabaseTable {
 public:
  TestTable() = default;

  TestTable(const TestTable&) = delete;
  TestTable& operator=(const TestTable&) = delete;

 protected:
  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override {
    static int table_key = 0;
    return reinterpret_cast<void*>(&table_key);
  }

  bool CreateTablesIfNecessary() override { return true; }

  bool MigrateToVersion(int version, bool* update_compatible_version) override {
    return true;
  }
};

class WebDataServiceBaseTest : public testing::Test {
 public:
  WebDataServiceBaseTest()
      : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)) {}

 protected:
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(WebDataServiceBaseTest, InitFailureCallback) {
  // Create a WebDatabaseService with an invalid path. This should fail to
  // initialize on the DB sequence.
  auto wdbs = base::MakeRefCounted<WebDatabaseService>(
      base::FilePath(), base::SequencedTaskRunner::GetCurrentDefault(),
      base::SequencedTaskRunner::GetCurrentDefault());

  wdbs->AddTable(std::make_unique<TestTable>());
  wdbs->LoadDatabase(os_crypt_.get());

  auto wdsb = base::MakeRefCounted<WebDataServiceBase>(
      wdbs, base::SequencedTaskRunner::GetCurrentDefault());

  std::optional<sql::InitStatus> status;
  base::RunLoop runloop;
  wdsb->Init(
      base::BindLambdaForTesting([&](sql::InitStatus s, const std::string&) {
        status = s;
        runloop.Quit();
      }));
  runloop.Run();
  ASSERT_TRUE(status.has_value());
  EXPECT_EQ(*status, sql::InitStatus::INIT_FAILURE);
}

}  // namespace
