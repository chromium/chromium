// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aggregation_service/aggregation_service_storage.h"

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "content/browser/aggregation_service/aggregation_service_test_utils.h"
#include "content/browser/aggregation_service/public_key.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class AggregationServiceStorageTest : public testing::Test {
 public:
  AggregationServiceStorageTest()
      : storage_(std::make_unique<AggregationServiceStorage>()) {}

 protected:
  std::unique_ptr<AggregationServiceStorage> storage_;
};

TEST_F(AggregationServiceStorageTest, GetSetPublicKeys) {
  std::vector<PublicKey> expected_keys{
      PublicKey("abcd", kABCD1234AsBytes,
                base::Time::FromJavaTime(1623000000000),
                base::Time::FromJavaTime(1624000000000)),
      PublicKey("bcde", kEFGH5678AsBytes,
                base::Time::FromJavaTime(1624000000000),
                base::Time::FromJavaTime(1625000000000)),
  };

  url::Origin origin = url::Origin::Create(GURL("https://a.com"));

  storage_->SetPublicKeys(content::PublicKeysForOrigin(origin, expected_keys));
  PublicKeysForOrigin keys = storage_->GetPublicKeys(origin);
  EXPECT_TRUE(aggregation_service::PublicKeysEqual(expected_keys, keys.keys));

  storage_->ClearPublicKeys(origin);
  keys = storage_->GetPublicKeys(origin);
  EXPECT_TRUE(keys.keys.empty());
}

}  // namespace content