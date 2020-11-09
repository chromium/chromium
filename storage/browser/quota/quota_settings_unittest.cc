// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "storage/browser/quota/quota_device_info_helper.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_settings.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace {

constexpr int64_t kLowPhysicalMemory = 1024 * 1024;
constexpr int64_t kHighPhysicalMemory = 65536 * kLowPhysicalMemory;

}  // namespace

namespace storage {

class MockQuotaDeviceInfoHelper : public QuotaDeviceInfoHelper {
 public:
  MockQuotaDeviceInfoHelper() = default;
  MOCK_CONST_METHOD1(AmountOfTotalDiskSpace, int64_t(const base::FilePath&));
  MOCK_CONST_METHOD0(AmountOfPhysicalMemory, int64_t());
};

class QuotaSettingsTest : public testing::Test {
 public:
  QuotaSettingsTest() = default;
  void SetUp() override { ASSERT_TRUE(data_dir_.CreateUniqueTempDir()); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  const base::FilePath& profile_path() const { return data_dir_.GetPath(); }

 private:
  base::ScopedTempDir data_dir_;
  QuotaSettings quota_settings_;
  DISALLOW_COPY_AND_ASSIGN(QuotaSettingsTest);
};

class QuotaSettingsIncognitoTest : public QuotaSettingsTest {
 public:
  QuotaSettingsIncognitoTest() = default;

 protected:
  void SetUpDeviceInfoHelper(const int expected_calls,
                             const int64_t physical_memory_amount) {
    ON_CALL(device_info_helper_, AmountOfPhysicalMemory())
        .WillByDefault(::testing::Return(physical_memory_amount));
    EXPECT_CALL(device_info_helper_, AmountOfPhysicalMemory())
        .Times(expected_calls);
  }

  void GetAndTestSettings(const int64_t physical_memory_amount) {
    bool callback_executed = false;
    GetNominalDynamicSettings(
        profile_path(), true, device_info_helper(),
        base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
          callback_executed = true;
          EXPECT_LE(physical_memory_amount *
                        GetIncognitoQuotaRatioLowerBound_ForTesting(),
                    settings->pool_size);
          EXPECT_GE(physical_memory_amount *
                        GetIncognitoQuotaRatioUpperBound_ForTesting(),
                    settings->pool_size);
        }));
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(callback_executed);
  }

  MockQuotaDeviceInfoHelper* device_info_helper() {
    return &device_info_helper_;
  }

 private:
  MockQuotaDeviceInfoHelper device_info_helper_;
};

TEST_F(QuotaSettingsTest, Default) {
  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfTotalDiskSpace(_))
      .WillByDefault(::testing::Return(2000));

  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), false, &device_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        ASSERT_NE(settings, base::nullopt);
        // 1600 = 2000 * default PoolSizeRatio (0.8)
        EXPECT_EQ(settings->pool_size, 1600);
        // 1200 = 1600 * default PerHostRatio (.75)
        EXPECT_EQ(settings->per_host_quota, 1200);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}

TEST_F(QuotaSettingsIncognitoTest, IncognitoDynamicQuota_LowPhysicalMemory) {
  const int expected_device_info_calls = 1;

  SetUpDeviceInfoHelper(expected_device_info_calls, kLowPhysicalMemory);
  GetAndTestSettings(kLowPhysicalMemory);
}

TEST_F(QuotaSettingsIncognitoTest, IncognitoDynamicQuota_HighPhysicalMemory) {
  const int expected_device_info_calls = 1;

  SetUpDeviceInfoHelper(expected_device_info_calls, kHighPhysicalMemory);
  GetAndTestSettings(kHighPhysicalMemory);
}

}  // namespace storage
