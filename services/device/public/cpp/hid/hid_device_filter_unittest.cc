// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "build/build_config.h"
#include "services/device/hid/hid_device_info.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"
#include "services/device/public/cpp/test/test_report_descriptors.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

#if BUILDFLAG(IS_MAC)
const uint64_t kTestDeviceId = 42;
#elif BUILDFLAG(IS_WIN)
const wchar_t* kTestDeviceId = L"device1";
#else
const char* kTestDeviceId = "device1";
#endif

}  // namespace

class HidFilterTest : public testing::Test {
 public:
  void SetUp() override {
    device_info_ = new HidDeviceInfo(
        kTestDeviceId, "1", 0x046d, 0xc31c, "Test Keyboard", "123ABC",
        mojom::HidBusType::kHIDBusTypeUSB, TestReportDescriptors::Keyboard());
  }

 protected:
  scoped_refptr<HidDeviceInfo> device_info_;
};

TEST_F(HidFilterTest, MatchAny) {
  HidDeviceFilter filter;
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchVendorId) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchVendorIdNegative) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x18d1);
  ASSERT_FALSE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchProductId) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  filter.SetProductId(0xc31c);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchProductIdNegative) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  filter.SetProductId(0x0801);
  ASSERT_FALSE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchUsagePage) {
  HidDeviceFilter filter;
  filter.SetUsagePage(mojom::kPageGenericDesktop);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchUsagePageNegative) {
  HidDeviceFilter filter;
  filter.SetUsagePage(mojom::kPageLed);
  ASSERT_FALSE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchVendorAndUsagePage) {
  HidDeviceFilter filter;
  filter.SetVendorId(0x046d);
  filter.SetUsagePage(mojom::kPageGenericDesktop);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchUsageAndPage) {
  HidDeviceFilter filter;
  filter.SetUsagePage(mojom::kPageGenericDesktop);
  filter.SetUsage(mojom::kGenericDesktopKeyboard);
  ASSERT_TRUE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchUsageAndPageNegative) {
  HidDeviceFilter filter;
  filter.SetUsagePage(mojom::kPageGenericDesktop);
  filter.SetUsage(0x02);
  ASSERT_FALSE(filter.Matches(*device_info_->device()));
}

TEST_F(HidFilterTest, MatchEmptyFilterListNegative) {
  std::vector<HidDeviceFilter> filters;
  ASSERT_FALSE(HidDeviceFilter::MatchesAny(*device_info_->device(), filters));
}

TEST_F(HidFilterTest, MatchFilterList) {
  std::vector<HidDeviceFilter> filters;
  HidDeviceFilter filter;
  filter.SetUsagePage(mojom::kPageGenericDesktop);
  filters.push_back(filter);
  ASSERT_TRUE(HidDeviceFilter::MatchesAny(*device_info_->device(), filters));
}

TEST_F(HidFilterTest, MatchFilterListNegative) {
  std::vector<HidDeviceFilter> filters;
  HidDeviceFilter filter;
  filter.SetUsagePage(mojom::kPageLed);
  filters.push_back(filter);
  ASSERT_FALSE(HidDeviceFilter::MatchesAny(*device_info_->device(), filters));
}

}  // namespace device
