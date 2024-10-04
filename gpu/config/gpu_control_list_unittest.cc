// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <vector>

#include "gpu/config/gpu_control_list.h"
#include "gpu/config/gpu_control_list_testing_data.h"
#include "gpu/config/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kOsVersion[] = "10.6.4";
const uint32_t kIntelVendorId = 0x8086;
const uint32_t kNvidiaVendorId = 0x10de;

#define LONG_STRING_CONST(...) #__VA_ARGS__

#define EXPECT_EMPTY_SET(feature_set) EXPECT_EQ(0u, feature_set.size())
#define EXPECT_SINGLE_FEATURE(feature_set, feature) \
    EXPECT_TRUE(feature_set.size() == 1 && feature_set.count(feature) == 1)

namespace gpu {

class GpuControlListTest : public testing::Test,
                           public testing::WithParamInterface<bool> {
 public:
  typedef GpuControlList::Entry Entry;

  GpuControlListTest() = default;
  ~GpuControlListTest() override = default;

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  std::unique_ptr<GpuControlList> Create(base::span<const Entry> entries) {
    std::unique_ptr<GpuControlList> rt(new GpuControlList(entries));
    rt->AddSupportedFeature("test_feature_0", TEST_FEATURE_0);
    rt->AddSupportedFeature("test_feature_1", TEST_FEATURE_1);
    rt->AddSupportedFeature("test_feature_2", TEST_FEATURE_2);
    return rt;
  }

  bool is_angle() const { return GetParam(); }

 protected:
  void SetUp() override {
    gpu_info_.gpu.vendor_id = kNvidiaVendorId;
    gpu_info_.gpu.device_id = 0x0640;
    gpu_info_.gpu.driver_vendor = "NVIDIA";
    gpu_info_.gpu.driver_version = "1.6.18";
    gpu_info_.machine_model_name = "MacBookPro";
    gpu_info_.machine_model_version = "7.1";
    if (is_angle()) {
      gpu_info_.gl_vendor = "Google Inc. (NVIDIA Corporation)";
      gpu_info_.gl_renderer =
          "ANGLE (NVIDIA Corporation, NVIDIA GeForce GT 120 OpenGL Engine,)";
      gpu_info_.gl_renderer = "";
    } else {
      gpu_info_.gl_vendor = "NVIDIA Corporation";
      gpu_info_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
    }
  }

  void TearDown() override {}

 private:
  GPUInfo gpu_info_;
};

INSTANTIATE_TEST_SUITE_P(,
                         GpuControlListTest,
                         testing::Values(false, true),
                         testing::PrintToStringParamName());

TEST_P(GpuControlListTest, NeedsMoreInfo) {
  const Entry kEntries[1] = {
      GetGpuControlListTestingEntries()[kGpuControlListTest_NeedsMoreInfo]};
  std::unique_ptr<GpuControlList> control_list = Create(kEntries);

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kNvidiaVendorId;

  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info);
  EXPECT_EMPTY_SET(features);
  EXPECT_TRUE(control_list->needs_more_info());
  std::vector<uint32_t> decision_entries = control_list->GetActiveEntries();
  EXPECT_EQ(0u, decision_entries.size());

  gpu_info.gpu.driver_version = "11";
  features = control_list->MakeDecision(
      GpuControlList::kOsWin, kOsVersion, gpu_info);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(control_list->needs_more_info());
  decision_entries = control_list->GetActiveEntries();
  EXPECT_EQ(1u, decision_entries.size());
  EXPECT_EQ(0u, decision_entries[0]);
  std::vector<uint32_t> entry_ids =
      control_list->GetEntryIDsFromIndices(decision_entries);
  EXPECT_EQ(1u, entry_ids.size());
  EXPECT_EQ(static_cast<uint32_t>(kGpuControlListTest_NeedsMoreInfo + 1),
            entry_ids[0]);
}

TEST_P(GpuControlListTest, NeedsMoreInfoForExceptions) {
  const Entry kEntries[1] = {
      GetGpuControlListTestingEntries()
          [kGpuControlListTest_NeedsMoreInfoForExceptions]};
  std::unique_ptr<GpuControlList> control_list = Create(kEntries);

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kIntelVendorId;

  // The case this entry does not apply.
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsMacosx, kOsVersion, gpu_info);
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case this entry might apply, but need more info.
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  // Ignore exceptions if main entry info matches
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_TRUE(control_list->needs_more_info());

  // The case we have full info, and the exception applies (so the entry
  // does not apply).
  gpu_info.gl_renderer = is_angle() ? "ANGLE (vendor, mesa, version)" : "mesa";
  features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case we have full info, and this entry applies.
  gpu_info.gl_renderer =
      is_angle() ? "ANGLE (vendor, my renderer, version)" : "my renderer";
  features = control_list->MakeDecision(GpuControlList::kOsLinux, kOsVersion,
      gpu_info);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(control_list->needs_more_info());
}

TEST_P(GpuControlListTest, IgnorableEntries) {
  // If an entry will not change the control_list decisions, then it should not
  // trigger the needs_more_info flag.
  const Entry kEntries[2] = {
      GetGpuControlListTestingEntries()[kGpuControlListTest_IgnorableEntries_0],
      GetGpuControlListTestingEntries()
          [kGpuControlListTest_IgnorableEntries_1]};
  std::unique_ptr<GpuControlList> control_list = Create(kEntries);

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kIntelVendorId;

  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsLinux, kOsVersion, gpu_info);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(control_list->needs_more_info());
}

TEST_P(GpuControlListTest, DisabledExtensionTest) {
  // exact setting.
  const Entry kEntries[2] = {GetGpuControlListTestingEntries()
                                 [kGpuControlListTest_DisabledExtensionTest_0],
                             GetGpuControlListTestingEntries()
                                 [kGpuControlListTest_DisabledExtensionTest_1]};
  std::unique_ptr<GpuControlList> control_list = Create(kEntries);

  GPUInfo gpu_info;
  control_list->MakeDecision(GpuControlList::kOsWin, kOsVersion, gpu_info);

  std::vector<std::string> disabled_extensions =
      control_list->GetDisabledExtensions();

  ASSERT_EQ(3u, disabled_extensions.size());
  ASSERT_STREQ("test_extension1", disabled_extensions[0].c_str());
  ASSERT_STREQ("test_extension2", disabled_extensions[1].c_str());
  ASSERT_STREQ("test_extension3", disabled_extensions[2].c_str());
}

TEST_P(GpuControlListTest, LinuxKernelVersion) {
  const Entry kEntries[1] = {GetGpuControlListTestingEntries()
                                 [kGpuControlListTest_LinuxKernelVersion]};
  std::unique_ptr<GpuControlList> control_list = Create(kEntries);

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;

  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsLinux, "3.13.0-63-generic", gpu_info);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);

  features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                        "3.19.2-1-generic", gpu_info);
  EXPECT_EMPTY_SET(features);
}

TEST_P(GpuControlListTest, TestGroup) {
  const Entry kEntries[3] = {
      GetGpuControlListTestingEntries()[kGpuControlListTest_LinuxKernelVersion],
      GetGpuControlListTestingEntries()[kGpuControlListTest_TestGroup_0],
      GetGpuControlListTestingEntries()[kGpuControlListTest_TestGroup_1]};
  std::unique_ptr<GpuControlList> control_list = Create(kEntries);
  GPUInfo gpu_info;

  // Default test group.
  std::set<int> features = control_list->MakeDecision(
      GpuControlList::kOsLinux, "3.13.2-1-generic", gpu_info);
  EXPECT_EMPTY_SET(features);

  // Test group 0, the default test group
  features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                        "3.13.2-1-generic", gpu_info, 0);
  EXPECT_EMPTY_SET(features);

  // Test group 1.
  features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                        "3.13.2-1-generic", gpu_info, 1);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);

  // Test group 2.
  features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                        "3.13.2-1-generic", gpu_info, 2);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_1);
}

}  // namespace gpu
