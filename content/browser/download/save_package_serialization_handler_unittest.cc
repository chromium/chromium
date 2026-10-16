// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_package_serialization_handler.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class SavePackageSerializationHandlerTest : public testing::Test {
 protected:
  SavePackageSerializationHandlerTest()
      : handler_(base::BindRepeating(
                     &SavePackageSerializationHandlerTest::OnDidReceiveData,
                     base::Unretained(this)),
                 base::BindOnce(&SavePackageSerializationHandlerTest::OnDone,
                                base::Unretained(this))) {}

  void OnDidReceiveData(const std::string& data) {
    received_data_.push_back(data);
  }

  void OnDone() { ++done_count_; }

  SavePackageSerializationHandler handler_;
  std::vector<std::string> received_data_;
  int done_count_ = 0;
};

TEST_F(SavePackageSerializationHandlerTest, ForwardsDataAndDone) {
  handler_.DidReceiveData("first");
  handler_.DidReceiveData("second");
  EXPECT_EQ(received_data_, std::vector<std::string>({"first", "second"}));
  EXPECT_EQ(done_count_, 0);

  handler_.Done();
  EXPECT_EQ(done_count_, 1);
}

TEST_F(SavePackageSerializationHandlerTest, IgnoresDataAfterDone) {
  handler_.DidReceiveData("before");
  handler_.Done();
  handler_.DidReceiveData("after");

  EXPECT_EQ(received_data_, std::vector<std::string>({"before"}));
  EXPECT_EQ(done_count_, 1);
}

TEST_F(SavePackageSerializationHandlerTest, IgnoresExtraDone) {
  handler_.Done();
  handler_.Done();

  EXPECT_TRUE(received_data_.empty());
  EXPECT_EQ(done_count_, 1);
}

}  // namespace

}  // namespace content
