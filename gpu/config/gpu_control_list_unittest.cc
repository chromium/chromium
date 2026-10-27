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

#if BUILDFLAG(IS_WIN)
#include <d3dcommon.h>
#endif

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

  GpuControlList::GLType GetGLType(const std::string& gl_renderer) {
    return GpuControlList::ProcessANGLEGLRenderer(gl_renderer);
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

  std::set<int> features = control_list->MakeDecision(GpuControlList::kOsWin,
                                                      kOsVersion, gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_TRUE(control_list->needs_more_info());
  std::vector<uint32_t> decision_entries = control_list->GetActiveEntries();
  EXPECT_EQ(0u, decision_entries.size());

  gpu_info.gpu.driver_version = "11";
  features = control_list->MakeDecision(GpuControlList::kOsWin, kOsVersion,
                                        gpu_info, {});
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
  std::set<int> features = control_list->MakeDecision(GpuControlList::kOsMacosx,
                                                      kOsVersion, gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case this entry might apply, but need more info.
  features = control_list->MakeDecision(GpuControlList::kOsLinux, kOsVersion,
                                        gpu_info, {});
  // Ignore exceptions if main entry info matches
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_TRUE(control_list->needs_more_info());

  // The case we have full info, and the exception applies (so the entry
  // does not apply).
  gpu_info.gl_renderer = is_angle() ? "ANGLE (vendor, mesa, version)" : "mesa";
  features = control_list->MakeDecision(GpuControlList::kOsLinux, kOsVersion,
                                        gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(control_list->needs_more_info());

  // The case we have full info, and this entry applies.
  gpu_info.gl_renderer =
      is_angle() ? "ANGLE (vendor, my renderer, version)" : "my renderer";
  features = control_list->MakeDecision(GpuControlList::kOsLinux, kOsVersion,
                                        gpu_info, {});
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

  std::set<int> features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                                      kOsVersion, gpu_info, {});
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(control_list->needs_more_info());
}

TEST_P(GpuControlListTest, IgnoredEntries) {
  const Entry kEntries[2] = {
      GetGpuControlListTestingEntries()[kGpuControlListTest_IgnorableEntries_0],
      GetGpuControlListTestingEntries()
          [kGpuControlListEntryTest_DirectRendering]};
  std::unique_ptr<GpuControlList> control_list = Create(kEntries);

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kIntelVendorId;
  gpu_info.direct_rendering_version = "2.0";

  // By default, both entries apply.
  std::set<int> features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                                      kOsVersion, gpu_info, {});
  EXPECT_EQ(2u, features.size());
  EXPECT_EQ(1u, features.count(TEST_FEATURE_0));
  EXPECT_EQ(1u, features.count(TEST_FEATURE_1));

  // Skip the first entry.
  std::vector<uint32_t> ignored_entries;
  ignored_entries.push_back(kEntries[0].id);
  features = control_list->MakeDecision(GpuControlList::kOsLinux, kOsVersion,
                                        gpu_info, ignored_entries);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_1);

  // Skip the second entry.
  ignored_entries.clear();
  ignored_entries.push_back(kEntries[1].id);
  features = control_list->MakeDecision(GpuControlList::kOsLinux, kOsVersion,
                                        gpu_info, ignored_entries);
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);

  // Skip both entries.
  ignored_entries.clear();
  ignored_entries.push_back(kEntries[0].id);
  ignored_entries.push_back(kEntries[1].id);
  features = control_list->MakeDecision(GpuControlList::kOsLinux, kOsVersion,
                                        gpu_info, ignored_entries);
  EXPECT_EMPTY_SET(features);
}

TEST_P(GpuControlListTest, DisabledExtensionTest) {
  // exact setting.
  const Entry kEntries[2] = {GetGpuControlListTestingEntries()
                                 [kGpuControlListTest_DisabledExtensionTest_0],
                             GetGpuControlListTestingEntries()
                                 [kGpuControlListTest_DisabledExtensionTest_1]};
  std::unique_ptr<GpuControlList> control_list = Create(kEntries);

  GPUInfo gpu_info;
  control_list->MakeDecision(GpuControlList::kOsWin, kOsVersion, gpu_info, {});

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
      GpuControlList::kOsLinux, "3.13.0-63-generic", gpu_info, {});
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);

  features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                        "3.19.2-1-generic", gpu_info, {});
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
      GpuControlList::kOsLinux, "3.13.2-1-generic", gpu_info, 0, {});
  EXPECT_EMPTY_SET(features);

  // Test group 0, the default test group
  features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                        "3.13.2-1-generic", gpu_info, 0, {});
  EXPECT_EMPTY_SET(features);

  // Test group 1.
  features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                        "3.13.2-1-generic", gpu_info, 1, {});
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);

  // Test group 2.
  features = control_list->MakeDecision(GpuControlList::kOsLinux,
                                        "3.13.2-1-generic", gpu_info, 2, {});
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_1);
}

TEST_P(GpuControlListTest, AngleVulkan) {
  EXPECT_EQ(GpuControlList::kGLTypeANGLE_VULKAN,
            GetGLType("ANGLE (ARM, Vulkan 1.3.247 (Mali-G52 (0x74021000)), "
                      "Mali G52-44.1.0)"));

  EXPECT_EQ(
      GpuControlList::kGLTypeANGLE_VULKAN,
      GetGLType("ANGLE (Intel, Vulkan 1.3.289 (Intel(R) Graphics (ADL GT2) "
                "(0x00004626)), Intel open-source Mesa driver-24.2.0)"));

  EXPECT_EQ(GpuControlList::kGLTypeANGLE_GLES,
            GetGLType("ANGLE (ARM, Mali-G52, OpenGL ES 3.1 vxxxxx)"));

  EXPECT_EQ(GpuControlList::kGLTypeGLES, GetGLType("Mali-G52"));
}

TEST_P(GpuControlListTest, ActiveGpuAndIsActiveGpuSpecified) {
  {
    // Single GPU (active = false) -> IsActiveGpuSpecified() is true,
    // active_gpu() returns gpu.
    GPUInfo gpu_info;
    gpu_info.gpu.vendor_id = kNvidiaVendorId;
    gpu_info.gpu.device_id = 0x0640;
    gpu_info.gpu.active = false;
    EXPECT_EQ(1u, gpu_info.GpuCount());
    EXPECT_TRUE(gpu_info.IsActiveGpuSpecified());
    EXPECT_EQ(gpu_info.gpu.vendor_id, gpu_info.active_gpu().vendor_id);
    EXPECT_EQ(gpu_info.gpu.device_id, gpu_info.active_gpu().device_id);
  }
  {
    // Hardware GPU as gpu + software renderer (vendor_id = 0x1414,
    // device_id = 0x008c) in secondary_gpus, neither active -> GpuCount() == 1,
    // IsActiveGpuSpecified() is true, active_gpu() returns gpu.
    GPUInfo gpu_info;
    gpu_info.gpu.vendor_id = kNvidiaVendorId;
    gpu_info.gpu.device_id = 0x0640;
    gpu_info.gpu.active = false;
    GPUInfo::GPUDevice sw_gpu;
    sw_gpu.vendor_id = 0x1414;
    sw_gpu.device_id = 0x008c;
    sw_gpu.active = false;
    gpu_info.secondary_gpus.push_back(sw_gpu);
    EXPECT_EQ(1u, gpu_info.GpuCount());
    EXPECT_TRUE(gpu_info.IsActiveGpuSpecified());
    EXPECT_EQ(gpu_info.gpu.vendor_id, gpu_info.active_gpu().vendor_id);
    EXPECT_EQ(gpu_info.gpu.device_id, gpu_info.active_gpu().device_id);
  }
  {
    // Software renderer (vendor_id = 0x1414, device_id = 0x008c) as gpu +
    // hardware GPU in secondary_gpus, neither active -> GpuCount() == 1,
    // IsActiveGpuSpecified() is true, active_gpu() returns secondary_gpus[0].
    GPUInfo gpu_info;
    gpu_info.gpu.vendor_id = 0x1414;
    gpu_info.gpu.device_id = 0x008c;
    gpu_info.gpu.active = false;
    GPUInfo::GPUDevice hw_gpu;
    hw_gpu.vendor_id = kIntelVendorId;
    hw_gpu.device_id = 0x0166;
    hw_gpu.active = false;
    gpu_info.secondary_gpus.push_back(hw_gpu);
    EXPECT_EQ(1u, gpu_info.GpuCount());
    EXPECT_TRUE(gpu_info.IsActiveGpuSpecified());
    EXPECT_EQ(gpu_info.secondary_gpus[0].vendor_id,
              gpu_info.active_gpu().vendor_id);
    EXPECT_EQ(gpu_info.secondary_gpus[0].device_id,
              gpu_info.active_gpu().device_id);
  }
  {
    // Dual hardware GPUs (gpu and secondary_gpus[0]), neither active ->
    // GpuCount() == 2, IsActiveGpuSpecified() is false, active_gpu() returns
    // gpu.
    GPUInfo gpu_info;
    gpu_info.gpu.vendor_id = kNvidiaVendorId;
    gpu_info.gpu.device_id = 0x0640;
    gpu_info.gpu.active = false;
    GPUInfo::GPUDevice second_gpu;
    second_gpu.vendor_id = kIntelVendorId;
    second_gpu.device_id = 0x0166;
    second_gpu.active = false;
    gpu_info.secondary_gpus.push_back(second_gpu);
    EXPECT_EQ(2u, gpu_info.GpuCount());
    EXPECT_FALSE(gpu_info.IsActiveGpuSpecified());
    EXPECT_EQ(gpu_info.gpu.vendor_id, gpu_info.active_gpu().vendor_id);
    EXPECT_EQ(gpu_info.gpu.device_id, gpu_info.active_gpu().device_id);

    // Dual hardware GPUs with gpu.active = true -> IsActiveGpuSpecified() is
    // true, active_gpu() returns gpu.
    gpu_info.gpu.active = true;
    gpu_info.secondary_gpus[0].active = false;
    EXPECT_TRUE(gpu_info.IsActiveGpuSpecified());
    EXPECT_EQ(gpu_info.gpu.vendor_id, gpu_info.active_gpu().vendor_id);
    EXPECT_EQ(gpu_info.gpu.device_id, gpu_info.active_gpu().device_id);

    // Dual hardware GPUs with secondary_gpus[0].active = true ->
    // IsActiveGpuSpecified() is true, active_gpu() returns secondary_gpus[0].
    gpu_info.gpu.active = false;
    gpu_info.secondary_gpus[0].active = true;
    EXPECT_TRUE(gpu_info.IsActiveGpuSpecified());
    EXPECT_EQ(gpu_info.secondary_gpus[0].vendor_id,
              gpu_info.active_gpu().vendor_id);
    EXPECT_EQ(gpu_info.secondary_gpus[0].device_id,
              gpu_info.active_gpu().device_id);
  }
}

TEST_P(GpuControlListTest, NeedsMoreInfoForActiveGpu) {
  const Entry kPrimaryEntries[1] = {
      GetGpuControlListTestingEntries()
          [kGpuControlListEntryDualGPUTest_ActivePrimaryGPU]};
  std::unique_ptr<GpuControlList> primary_list = Create(kPrimaryEntries);

  const Entry kSecondaryEntries[1] = {
      GetGpuControlListTestingEntries()
          [kGpuControlListEntryDualGPUTest_ActiveSecondaryGPU]};
  std::unique_ptr<GpuControlList> secondary_list = Create(kSecondaryEntries);

  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = kNvidiaVendorId;
  gpu_info.gpu.device_id = 0x0640;
  gpu_info.gpu.active = false;
  GPUInfo::GPUDevice second_gpu;
  second_gpu.vendor_id = kIntelVendorId;
  second_gpu.device_id = 0x0166;
  second_gpu.active = false;
  gpu_info.secondary_gpus.push_back(second_gpu);

  // With dual hardware GPUs (0x10de:0x0640 primary, 0x8086:0x0166 secondary)
  // and neither marked active: for both entries,
  // MakeDecision(GpuControlList::kOsMacosx, kOsVersion, gpu_info, {}) returns
  // empty features, GetActiveEntries() is empty, and
  // control_list->needs_more_info() is true.
  std::set<int> features = primary_list->MakeDecision(GpuControlList::kOsMacosx,
                                                      kOsVersion, gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_TRUE(primary_list->needs_more_info());
  EXPECT_EQ(0u, primary_list->GetActiveEntries().size());

  features = secondary_list->MakeDecision(GpuControlList::kOsMacosx, kOsVersion,
                                          gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_TRUE(secondary_list->needs_more_info());
  EXPECT_EQ(0u, secondary_list->GetActiveEntries().size());

  // When gpu.active = true: ActivePrimaryGPU returns TEST_FEATURE_0 with
  // needs_more_info() == false; ActiveSecondaryGPU returns empty features with
  // needs_more_info() == false.
  gpu_info.gpu.active = true;
  gpu_info.secondary_gpus[0].active = false;

  features = primary_list->MakeDecision(GpuControlList::kOsMacosx, kOsVersion,
                                        gpu_info, {});
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(primary_list->needs_more_info());

  features = secondary_list->MakeDecision(GpuControlList::kOsMacosx, kOsVersion,
                                          gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(secondary_list->needs_more_info());

  // When secondary_gpus[0].active = true: ActiveSecondaryGPU returns
  // TEST_FEATURE_0 with needs_more_info() == false; ActivePrimaryGPU returns
  // empty features with needs_more_info() == false.
  gpu_info.gpu.active = false;
  gpu_info.secondary_gpus[0].active = true;

  features = secondary_list->MakeDecision(GpuControlList::kOsMacosx, kOsVersion,
                                          gpu_info, {});
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(secondary_list->needs_more_info());

  features = primary_list->MakeDecision(GpuControlList::kOsMacosx, kOsVersion,
                                        gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(primary_list->needs_more_info());

  // When gpu is a software renderer (0x1414:0x008c) and secondary_gpus[0] is
  // 0x8086:0x0166 (active = false): ActiveSecondaryGPU returns TEST_FEATURE_0
  // with needs_more_info() == false.
  gpu_info.gpu.vendor_id = 0x1414;
  gpu_info.gpu.device_id = 0x008c;
  gpu_info.gpu.active = false;
  gpu_info.secondary_gpus[0].vendor_id = kIntelVendorId;
  gpu_info.secondary_gpus[0].device_id = 0x0166;
  gpu_info.secondary_gpus[0].active = false;

  features = secondary_list->MakeDecision(GpuControlList::kOsMacosx, kOsVersion,
                                          gpu_info, {});
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(secondary_list->needs_more_info());
}

#if BUILDFLAG(IS_WIN)
TEST_P(GpuControlListTest, D3DFeatureLevel) {
  const Entry kEntries[1] = {
      GetGpuControlListTestingEntries()[kGpuControlListEntryTest_D3DFeatureLevel]};
  std::unique_ptr<GpuControlList> control_list = Create(kEntries);
  GPUInfo gpu_info;

  // If d3d11_feature_level is unspecified (0), the entry does not apply yet,
  // but needs_more_info() should be true.
  std::set<int> features = control_list->MakeDecision(GpuControlList::kOsWin,
                                                      kOsVersion, gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_TRUE(control_list->needs_more_info());

  // D3D feature level 11.0. Entry requires < 12.0. So it applies.
  gpu_info.d3d11_feature_level = D3D_FEATURE_LEVEL_11_0;
  features = control_list->MakeDecision(GpuControlList::kOsWin, kOsVersion,
                                        gpu_info, {});
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(control_list->needs_more_info());

  gpu_info.d3d11_feature_level = D3D_FEATURE_LEVEL_11_1;
  features = control_list->MakeDecision(GpuControlList::kOsWin, kOsVersion,
                                        gpu_info, {});
  EXPECT_SINGLE_FEATURE(features, TEST_FEATURE_0);
  EXPECT_FALSE(control_list->needs_more_info());

  // D3D feature level 12.0. Entry requires < 12.0. So it does not apply.
  gpu_info.d3d11_feature_level = D3D_FEATURE_LEVEL_12_0;
  features = control_list->MakeDecision(GpuControlList::kOsWin, kOsVersion,
                                        gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(control_list->needs_more_info());

  gpu_info.d3d11_feature_level = D3D_FEATURE_LEVEL_12_1;
  features = control_list->MakeDecision(GpuControlList::kOsWin, kOsVersion,
                                        gpu_info, {});
  EXPECT_EMPTY_SET(features);
  EXPECT_FALSE(control_list->needs_more_info());
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace gpu
