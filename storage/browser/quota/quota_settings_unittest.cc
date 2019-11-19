// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "storage/browser/quota/quota_device_info_helper.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_settings.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

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

TEST_F(QuotaSettingsTest, ExpandedTempPool) {
  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfTotalDiskSpace(_))
      .WillByDefault(::testing::Return(2000));
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kQuotaExpandPoolSize,
      {{"PoolSizeRatio", "0.75"}, {"PerHostRatio", "0.5"}});

  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), false, &device_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        ASSERT_NE(settings, base::nullopt);
        // 1500 = 2000 * PoolSizeRatio
        EXPECT_EQ(settings->pool_size, 1500);
        // 750 = 1500 * PerHostRatio
        EXPECT_EQ(settings->per_host_quota, 750);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}

TEST_F(QuotaSettingsTest, UnlimitedTempPool) {
  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfTotalDiskSpace(_))
      .WillByDefault(::testing::Return(2000));
  scoped_feature_list_.InitAndEnableFeature(features::kQuotaUnlimitedPoolSize);

  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), false, &device_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        ASSERT_NE(settings, base::nullopt);
        EXPECT_EQ(settings->pool_size, 2000);
        EXPECT_EQ(settings->per_host_quota, 2000);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}

TEST_F(QuotaSettingsTest, IncognitoQuotaCapped) {
  const int64_t kMBytes = 1024 * 1024;
  const int64_t kMaxIncognitoPoolSize = 330 * kMBytes;  // 300 MB + 10%

  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfPhysicalMemory())
      .WillByDefault(::testing::Return(kMaxIncognitoPoolSize));

  scoped_feature_list_.InitAndDisableFeature(features::kIncognitoDynamicQuota);
  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), true, &device_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        EXPECT_GE(kMaxIncognitoPoolSize, settings->pool_size);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}

TEST_F(QuotaSettingsTest, IncognitoDynamicQuota1) {
  const int64_t kMBytes = 1024 * 1024;
  const int64_t kMaxIncognitoPoolSize = 330 * kMBytes;  // 300 MB + 10%
  const int64_t physical_memory_amount = kMaxIncognitoPoolSize / 10;

  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfPhysicalMemory())
      .WillByDefault(::testing::Return(physical_memory_amount));

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kIncognitoDynamicQuota,
      {{"IncognitoQuotaRatioLowerBound", "0.1"},
       {"IncognitoQuotaRatioUpperBound", "0.2"}});

  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), true, &device_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        EXPECT_LE(physical_memory_amount / 10, settings->pool_size);
        EXPECT_GE(physical_memory_amount / 5, settings->pool_size);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}

TEST_F(QuotaSettingsTest, IncognitoDynamicQuota2) {
  const int64_t kMBytes = 1024 * 1024;
  const int64_t kMaxIncognitoPoolSize = 330 * kMBytes;  // 300 MB + 10%
  const int64_t physical_memory_amount = kMaxIncognitoPoolSize;

  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfPhysicalMemory())
      .WillByDefault(::testing::Return(physical_memory_amount));

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kIncognitoDynamicQuota,
      {{"IncognitoQuotaRatioLowerBound", "0.1"},
       {"IncognitoQuotaRatioUpperBound", "0.2"}});

  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), true, &device_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        EXPECT_LE(physical_memory_amount / 10, settings->pool_size);
        EXPECT_GE(physical_memory_amount / 5, settings->pool_size);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}

TEST_F(QuotaSettingsTest, IncognitoDynamicQuota3) {
  const int64_t kMBytes = 1024 * 1024;
  const int64_t kMaxIncognitoPoolSize = 330 * kMBytes;  // 300 MB + 10%
  const int64_t physical_memory_amount = kMaxIncognitoPoolSize * 100;

  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfPhysicalMemory())
      .WillByDefault(::testing::Return(physical_memory_amount));

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kIncognitoDynamicQuota,
      {{"IncognitoQuotaRatioLowerBound", "0.1"},
       {"IncognitoQuotaRatioUpperBound", "0.2"}});

  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), true, &device_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        EXPECT_LE(physical_memory_amount / 10, settings->pool_size);
        EXPECT_GE(physical_memory_amount / 5, settings->pool_size);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}

TEST_F(QuotaSettingsTest, IncognitoDynamicQuota4) {
  const int64_t kMBytes = 1024 * 1024;
  const int64_t kMaxIncognitoPoolSize = 330 * kMBytes;  // 300 MB + 10%
  const int64_t physical_memory_amount = kMaxIncognitoPoolSize * 1000;

  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfPhysicalMemory())
      .WillByDefault(::testing::Return(physical_memory_amount));

  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kIncognitoDynamicQuota,
      {{"IncognitoQuotaRatioLowerBound", "0.1"},
       {"IncognitoQuotaRatioUpperBound", "0.2"}});

  bool callback_executed = false;
  GetNominalDynamicSettings(
      profile_path(), true, &device_info_helper,
      base::BindLambdaForTesting([&](base::Optional<QuotaSettings> settings) {
        callback_executed = true;
        EXPECT_LE(physical_memory_amount / 10, settings->pool_size);
        EXPECT_GE(physical_memory_amount / 5, settings->pool_size);
      }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_executed);
}

}  // namespace storage
