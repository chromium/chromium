// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/floss/floss_gatt_client.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace floss {

class FlossGattClientTest : public testing::Test {
 public:
  void SetUp() override { client_ = FlossGattClient::Create(); }

  void TearDown() override { client_.reset(); }

  std::unique_ptr<FlossGattClient> client_;

  base::test::TaskEnvironment task_environment_;
  base::WeakPtrFactory<FlossGattClientTest> weak_ptr_factory_{this};
};

TEST_F(FlossGattClientTest, BasicTest) {}

}  // namespace floss
