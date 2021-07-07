// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_aggregation_service_impl.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class TestAggregationServiceImplTest : public testing::Test {
 public:
  TestAggregationServiceImplTest()
      : impl_(std::make_unique<TestAggregationServiceImpl>()) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestAggregationServiceImpl> impl_;
};

TEST_F(TestAggregationServiceImplTest, SetPublicKeys) {
  std::string json_string = R"(
        {
            "1.0" : [
                {
                    "id" : "abcd",
                    "key" : "defg",
                    "not_before": "1623000000000",
                    "not_after" : "1624000000000"
                }
            ]
        }
    )";

  url::Origin origin = url::Origin::Create(GURL("https://a.com"));

  base::RunLoop set_loop;
  impl_->SetPublicKeys(origin, json_string,
                       base::BindLambdaForTesting([&](bool succeeded) {
                         EXPECT_TRUE(succeeded);
                         set_loop.Quit();
                       }));
  set_loop.Run();

  base::RunLoop get_loop;
  impl_->GetPublicKeys(
      origin, base::BindLambdaForTesting([&](PublicKeysForOrigin keys) {
        EXPECT_TRUE(content::aggregation_service::PublicKeysEqual(
            {content::PublicKey("abcd", "defg",
                                base::Time::FromJavaTime(1623000000000),
                                base::Time::FromJavaTime(1624000000000))},
            keys.keys));
        get_loop.Quit();
      }));
  get_loop.Run();
}

}  // namespace content
