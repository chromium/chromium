// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/web_database_service.h"

#include <array>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ResultOf;

using RequestMockCallback =
    base::MockOnceCallback<std::unique_ptr<WDTypedResult>(WebDatabase*)>;

using ResultFuture = base::test::TestFuture<WebDataServiceBase::Handle,
                                            std::unique_ptr<WDTypedResult>>;

template <typename T, typename Matcher>
testing::Matcher<std::unique_ptr<WDTypedResult>> ValueOfWDResult(
    Matcher&& matcher) {
  return ResultOf(
      [](const std::unique_ptr<WDTypedResult>& result) {
        return static_cast<WDResult<T>*>(result.get())->GetValue();
      },
      std::forward<Matcher>(matcher));
}

class TestTable : public WebDatabaseTable {
 public:
  TestTable() = default;
  TestTable(const TestTable&) = delete;
  TestTable& operator=(const TestTable&) = delete;
  ~TestTable() override = default;

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

class WebDatabaseServiceTest : public testing::Test {
 public:
  WebDatabaseServiceTest() = default;
  ~WebDatabaseServiceTest() override = default;

  void SetUp() override {
    wdbs_ = base::MakeRefCounted<WebDatabaseService>(
        base::FilePath(WebDatabase::kInMemoryPath),
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::SequencedTaskRunner::GetCurrentDefault());

    wdbs_->AddTable(std::make_unique<TestTable>());
    wdbs_->LoadDatabase(os_crypt_.get());
    WaitForEmptyDBSequence();
  }

  void TearDown() override {
    wdbs_->ShutdownDatabase();
    WaitForEmptyDBSequence();
  }

  void WaitForEmptyDBSequence() {
    base::RunLoop run_loop;
    wdbs_->GetDbSequence()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                             run_loop.QuitClosure());
    run_loop.Run();
  }

  WebDatabaseService& wdbs() { return *wdbs_; }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_ =
      os_crypt_async::GetTestOSCryptAsyncForTesting(
          /*is_sync_for_unittests=*/true);
  scoped_refptr<WebDatabaseService> wdbs_;
};

// Tests that tasks and their replies posted by ScheduleDBTaskWithResult() are
// run.
TEST_F(WebDatabaseServiceTest, ScheduleDBTaskWithResult_RunsTask) {
  RequestMockCallback request;
  ResultFuture result;
  EXPECT_CALL(request, Run(wdbs().GetDatabaseOnDB()))
      .WillOnce([](WebDatabase* db) {
        return std::make_unique<WDResult<int64_t>>(INT64_RESULT, 123);
      });
  wdbs().ScheduleDBTaskWithResult(FROM_HERE, request.Get(),
                                  result.GetCallback());
  EXPECT_THAT(result.Get<1>(), ValueOfWDResult<int64_t>(123));
}

// Tests that pending requests posted by ScheduleDBTaskWithResult() can be
// cancelled.
TEST_F(WebDatabaseServiceTest, ScheduleDBTaskWithResult_Cancelable) {
  // We start three requests and cancel the second one before any of them
  // finishes.
  std::array<RequestMockCallback, 3> requests{};
  std::array<ResultFuture, 3> results{};
  std::array<WebDataServiceBase::Handle, 3> handles{};

  EXPECT_CALL(requests[0], Run(wdbs().GetDatabaseOnDB()))
      .WillOnce([](WebDatabase* db) {
        return std::make_unique<WDResult<int64_t>>(INT64_RESULT, 0);
      });
  EXPECT_CALL(requests[2], Run(wdbs().GetDatabaseOnDB()))
      .WillOnce([](WebDatabase* db) {
        return std::make_unique<WDResult<int64_t>>(INT64_RESULT, 2);
      });

  handles[0] = wdbs().ScheduleDBTaskWithResult(FROM_HERE, requests[0].Get(),
                                               results[0].GetCallback());
  handles[1] = wdbs().ScheduleDBTaskWithResult(FROM_HERE, requests[1].Get(),
                                               results[1].GetCallback());
  handles[2] = wdbs().ScheduleDBTaskWithResult(FROM_HERE, requests[2].Get(),
                                               results[2].GetCallback());

  EXPECT_THAT(handles, ElementsAre(1, 2, 3));
  wdbs().CancelRequest(handles[1]);
  EXPECT_THAT(results[0].Get<1>(), ValueOfWDResult<int64_t>(0));
  // requests[1] got cancelled.
  EXPECT_THAT(results[2].Get<1>(), ValueOfWDResult<int64_t>(2));
}

}  // namespace
