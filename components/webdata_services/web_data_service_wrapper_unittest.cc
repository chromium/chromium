// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata_services/web_data_service_wrapper.h"

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/search_engines/keyword_table.h"
#include "components/search_engines/keyword_web_data_service.h"
#include "components/search_engines/template_url_data.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper class that stores request result.
class KeywordsConsumer : public WebDataServiceConsumer {
 public:
  ~KeywordsConsumer() override = default;

  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override {
    keywords_ = std::move(result);
  }

  std::unique_ptr<WDTypedResult> keywords_;
};

class WebDataServiceWrapperTest : public testing::Test {
 public:
  WebDataServiceWrapperTest()
      : os_crypt_(os_crypt_async::GetTestOSCryptAsyncForTesting(
            /*is_sync_for_unittests=*/true)) {}

 protected:
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  // Creates WebDataServiceWrapper under test.
  std::unique_ptr<WebDataServiceWrapper> CreateWebDataServiceWrapper() {
    return std::make_unique<WebDataServiceWrapper>(
        scoped_temp_dir_.GetPath(), "en_US",
        task_environment_.GetMainThreadTaskRunner(), base::DoNothing(),
        os_crypt_.get(),
        /*use_in_memory_autofill_account_database=*/false);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;
  base::ScopedTempDir scoped_temp_dir_;
};

}  // namespace

// This test ensures that KeywordWebDataService writes all pending operations
// on shutdown. This requires wrapped services to be closed in a proper order
// but also that asynchronous tasks are posted to DB sequence in a proper order
// (the task that closes database should be the last one).
TEST_F(WebDataServiceWrapperTest, ShutdownKeywordWebDataService) {
  TemplateURLData test_keyword;
  test_keyword.SetShortName(u"Foo Bar");
  test_keyword.SetKeyword(u"foo");
  test_keyword.SetURL("http://foo.bar");
  test_keyword.id = 1234;

  // Create WebDataServiceWrapper. Add a test keyword and perform shutdown.
  auto web_data_service_wrapper = CreateWebDataServiceWrapper();
  web_data_service_wrapper->GetKeywordWebData()->AddKeyword(test_keyword);
  web_data_service_wrapper->Shutdown();
  task_environment_.RunUntilIdle();
  web_data_service_wrapper.reset();

  // Create WebDataServiceWrapper again. Verify that the test keyword is present
  // in the database.
  web_data_service_wrapper = CreateWebDataServiceWrapper();
  KeywordsConsumer keywords_consumer;
  web_data_service_wrapper->GetKeywordWebData()->GetKeywords(
      &keywords_consumer);
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(keywords_consumer.keywords_);
  ASSERT_EQ(KEYWORDS_RESULT, keywords_consumer.keywords_->GetType());
  WDKeywordsResult keyword_result =
      reinterpret_cast<const WDResult<WDKeywordsResult>*>(
          keywords_consumer.keywords_.get())
          ->GetValue();
  ASSERT_EQ(1u, keyword_result.keywords.size());
  EXPECT_EQ(test_keyword.short_name(), keyword_result.keywords[0].short_name());
  web_data_service_wrapper->Shutdown();
}
