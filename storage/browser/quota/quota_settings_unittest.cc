// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_settings.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "storage/browser/quota/quota_device_info_helper.h"
#include "storage/browser/quota/quota_features.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;

namespace {

constexpr uint64_t kLowPhysicalMemory = 1024 * 1024;
constexpr uint64_t kHighPhysicalMemory = 65536 * kLowPhysicalMemory;

}  // namespace

namespace storage {

class MockQuotaDeviceInfoHelper : public QuotaDeviceInfoHelper {
 public:
  MockQuotaDeviceInfoHelper() = default;
  MOCK_CONST_METHOD1(AmountOfTotalDiskSpace, int64_t(const base::FilePath&));
  MOCK_CONST_METHOD0(AmountOfPhysicalMemory, uint64_t());
};

class QuotaSettingsTest : public testing::Test {
 public:
  QuotaSettingsTest() = default;
  void SetUp() override { ASSERT_TRUE(data_dir_.CreateUniqueTempDir()); }

  // Synchronous proxy to GetNominalDynamicSettings().
  std::optional<QuotaSettings> GetSettings(
      bool is_incognito,
      QuotaDeviceInfoHelper* device_info_helper) {
    std::optional<QuotaSettings> quota_settings;
    base::RunLoop run_loop;
    GetNominalDynamicSettings(
        profile_path(), is_incognito, device_info_helper,
        base::BindLambdaForTesting([&](std::optional<QuotaSettings> settings) {
          quota_settings = std::move(settings);
          run_loop.Quit();
        }));
    run_loop.Run();
    return quota_settings;
  }

  const base::FilePath& profile_path() const { return data_dir_.GetPath(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir data_dir_;
  base::test::TaskEnvironment task_environment_;
};

class QuotaSettingsIncognitoTest : public QuotaSettingsTest {
 public:
  QuotaSettingsIncognitoTest() = default;

 protected:
  void SetUpDeviceInfoHelper(const int expected_calls,
                             const uint64_t physical_memory_amount) {
    ON_CALL(device_info_helper_, AmountOfPhysicalMemory())
        .WillByDefault(::testing::Return(physical_memory_amount));
    EXPECT_CALL(device_info_helper_, AmountOfPhysicalMemory())
        .Times(expected_calls);
  }

  void GetAndTestSettings(const uint64_t physical_memory_amount) {
    std::optional<QuotaSettings> settings =
        GetSettings(true, &device_info_helper_);
    ASSERT_TRUE(settings.has_value());
    const uint64_t pool_size =
        base::checked_cast<uint64_t>(settings->pool_size);
    EXPECT_LE(
        physical_memory_amount * GetIncognitoQuotaRatioLowerBound_ForTesting(),
        pool_size);
    EXPECT_GE(
        physical_memory_amount * GetIncognitoQuotaRatioUpperBound_ForTesting(),
        pool_size);
  }

 private:
  MockQuotaDeviceInfoHelper device_info_helper_;
};

TEST_F(QuotaSettingsTest, Default) {
  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfTotalDiskSpace(_))
      .WillByDefault(::testing::Return(2000));

  std::optional<QuotaSettings> settings =
      GetSettings(false, &device_info_helper);
  ASSERT_TRUE(settings.has_value());
  // 1600 = 2000 * default PoolSizeRatio (0.8)
  EXPECT_EQ(settings->pool_size, 1600);
  // 1200 = 1600 * default PerStorageKeyRatio (.75)
  EXPECT_EQ(settings->per_storage_key_quota, 1200);
}

TEST_F(QuotaSettingsTest, FeatureParamsWithLargeFixedQuota) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kStorageQuotaSettings, {{"MustRemainAvailableBytes", "500"},
                                        {"MustRemainAvailableRatio", "0.01"},
                                        {"PoolSizeBytes", "2000"},
                                        {"PoolSizeRatio", "0.8"},
                                        {"ShouldRemainAvailableBytes", "600"},
                                        {"ShouldRemainAvailableRatio", "0.1"}});

  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfTotalDiskSpace(_))
      .WillByDefault(::testing::Return(2000));

  std::optional<QuotaSettings> settings =
      GetSettings(false, &device_info_helper);
  ASSERT_TRUE(settings.has_value());

  EXPECT_EQ(settings->pool_size, 1600);
  EXPECT_EQ(settings->must_remain_available, 20);
  EXPECT_EQ(settings->should_remain_available, 200);
}

TEST_F(QuotaSettingsTest, FeatureParamsWithSmallFixedQuota) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kStorageQuotaSettings, {{"MustRemainAvailableBytes", "5"},
                                        {"MustRemainAvailableRatio", "0.01"},
                                        {"PoolSizeBytes", "20"},
                                        {"PoolSizeRatio", "0.8"},
                                        {"ShouldRemainAvailableBytes", "60"},
                                        {"ShouldRemainAvailableRatio", "0.1"}});

  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfTotalDiskSpace(_))
      .WillByDefault(::testing::Return(2000));

  std::optional<QuotaSettings> settings =
      GetSettings(false, &device_info_helper);
  ASSERT_TRUE(settings.has_value());

  EXPECT_EQ(settings->pool_size, 20);
  EXPECT_EQ(settings->must_remain_available, 5);
  EXPECT_EQ(settings->should_remain_available, 60);
}

TEST_F(QuotaSettingsTest, FeatureParamsWithoutFixedQuota) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kStorageQuotaSettings, {{"MustRemainAvailableRatio", "0.01"},
                                        {"PoolSizeRatio", "0.8"},
                                        {"ShouldRemainAvailableRatio", "0.1"}});

  MockQuotaDeviceInfoHelper device_info_helper;
  ON_CALL(device_info_helper, AmountOfTotalDiskSpace(_))
      .WillByDefault(::testing::Return(2000));

  std::optional<QuotaSettings> settings =
      GetSettings(false, &device_info_helper);
  ASSERT_TRUE(settings.has_value());

  EXPECT_EQ(settings->pool_size, 1600);
  EXPECT_EQ(settings->must_remain_available, 20);
  EXPECT_EQ(settings->should_remain_available, 200);
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
