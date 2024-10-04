// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "gpu/config/gpu_control_list.h"
#include "gpu/config/gpu_control_list_testing_data.h"
#include "gpu/config/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

constexpr auto kOsLinux = GpuControlList::kOsLinux;
constexpr auto kOsMacosx = GpuControlList::kOsMacosx;
constexpr auto kOsWin = GpuControlList::kOsWin;
constexpr auto kOsChromeOS = GpuControlList::kOsChromeOS;
constexpr auto kOsAndroid = GpuControlList::kOsAndroid;
constexpr auto kOsFuchsia = GpuControlList::kOsFuchsia;
constexpr auto kOsAny = GpuControlList::kOsAny;

constexpr GpuControlList::OsType kAllOsType[] = {
    kOsMacosx, kOsWin, kOsLinux, kOsChromeOS, kOsAndroid, kOsFuchsia};

}  // namespace anonymous

class GpuControlListEntryTest : public testing::Test {
 public:
  typedef GpuControlList::Entry Entry;

  GpuControlListEntryTest() = default;
  ~GpuControlListEntryTest() override = default;

  const GPUInfo& gpu_info() const {
    return gpu_info_;
  }

  const Entry& GetEntry(size_t index) {
    EXPECT_LT(index, GetGpuControlListTestingEntries().size());
    EXPECT_EQ(index + 1, GetGpuControlListTestingEntries()[index].id);
    return GetGpuControlListTestingEntries()[index];
  }

  size_t CountFeature(const Entry& entry, int feature) {
    size_t count = 0;
    for (size_t ii = 0; ii < entry.features.size(); ++ii) {
      if (entry.features[ii] == feature) {
        ++count;
      }
    }
    return count;
  }

 protected:
  void SetUp() override {
    gpu_info_.gpu.vendor_id = 0x10de;
    gpu_info_.gpu.device_id = 0x0640;
    gpu_info_.gpu.active = true;
    gpu_info_.gpu.driver_vendor = "NVIDIA";
    gpu_info_.gpu.driver_version = "1.6.18";
    gpu_info_.gl_version = "2.1 NVIDIA-8.24.11 310.90.9b01";
    gpu_info_.gl_vendor = "NVIDIA Corporation";
    gpu_info_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
  }

  GPUInfo gpu_info_;
};

TEST_F(GpuControlListEntryTest, DetailedEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_DetailedEntry);
  EXPECT_EQ(kOsMacosx, entry.conditions.os_type);
  EXPECT_STREQ("GpuControlListEntryTest.DetailedEntry", entry.description);
  EXPECT_EQ(2u, entry.cr_bugs.size());
  EXPECT_EQ(1024u, entry.cr_bugs[0]);
  EXPECT_EQ(678u, entry.cr_bugs[1]);
  EXPECT_EQ(1u, entry.features.size());
  EXPECT_EQ(1u, CountFeature(entry, TEST_FEATURE_0));
  EXPECT_FALSE(entry.NeedsMoreInfo(gpu_info(), true));
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.6.4", gpu_info()));
  EXPECT_EQ(2u, entry.disabled_extensions.size());
  EXPECT_STREQ("test_extension1", entry.disabled_extensions[0]);
  EXPECT_STREQ("test_extension2", entry.disabled_extensions[1]);
}

TEST_F(GpuControlListEntryTest, VendorOnAllOsEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_VendorOnAllOsEntry);
  EXPECT_EQ(kOsAny, entry.conditions.os_type);
  for (auto os_type : kAllOsType)
    EXPECT_TRUE(entry.Contains(os_type, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, VendorOnLinuxEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_VendorOnLinuxEntry);
  EXPECT_EQ(kOsLinux, entry.conditions.os_type);
  const GpuControlList::OsType os_types[] = {kOsMacosx, kOsWin, kOsChromeOS,
                                             kOsAndroid, kOsFuchsia};
  for (auto os_type : os_types)
    EXPECT_FALSE(entry.Contains(os_type, "10.6", gpu_info()));
  EXPECT_TRUE(entry.Contains(kOsLinux, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, AllExceptNVidiaOnLinuxEntry) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_AllExceptNVidiaOnLinuxEntry);
  EXPECT_EQ(kOsLinux, entry.conditions.os_type);
  const GpuControlList::OsType os_types[] = {kOsMacosx, kOsWin, kOsLinux,
                                             kOsChromeOS, kOsAndroid};
  for (auto os_type : os_types) {
    EXPECT_FALSE(entry.Contains(os_type, "10.6", gpu_info()));
  }
}

TEST_F(GpuControlListEntryTest, AllExceptIntelOnLinuxEntry) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_AllExceptIntelOnLinuxEntry);
  EXPECT_EQ(kOsLinux, entry.conditions.os_type);
  const GpuControlList::OsType os_types[] = {kOsMacosx, kOsWin, kOsChromeOS,
                                             kOsAndroid, kOsFuchsia};
  for (auto os_type : os_types)
    EXPECT_FALSE(entry.Contains(os_type, "10.6", gpu_info()));
  EXPECT_TRUE(entry.Contains(kOsLinux, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, MultipleDevicesEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_MultipleDevicesEntry);
  EXPECT_EQ(kOsAny, entry.conditions.os_type);
  for (auto os_type : kAllOsType)
    EXPECT_TRUE(entry.Contains(os_type, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, ChromeOSEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_ChromeOSEntry);
  EXPECT_EQ(kOsChromeOS, entry.conditions.os_type);
  const GpuControlList::OsType os_types[] = {kOsMacosx, kOsWin, kOsLinux,
                                             kOsAndroid, kOsFuchsia};
  for (auto os_type : os_types)
    EXPECT_FALSE(entry.Contains(os_type, "10.6", gpu_info()));
  EXPECT_TRUE(entry.Contains(kOsChromeOS, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, GlVersionGLESEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GlVersionGLESEntry);
  GPUInfo gpu_info;
  gpu_info.gl_version = "OpenGL ES 3.0 V@66.0 AU@ (CL@)";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.4.2", gpu_info));
  gpu_info.gl_version = "OpenGL ES 3.0V@66.0 AU@ (CL@)";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.4.2", gpu_info));
  gpu_info.gl_version = "OpenGL ES 3.1 V@66.0 AU@ (CL@)";
  EXPECT_FALSE(entry.Contains(kOsAndroid, "4.4.2", gpu_info));
  gpu_info.gl_version = "3.0 NVIDIA-8.24.11 310.90.9b01";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_version = "OpenGL ES 3.0 (ANGLE 1.2.0.2450)";
  EXPECT_FALSE(entry.Contains(kOsWin, "6.1", gpu_info));
}

TEST_F(GpuControlListEntryTest, GlVersionANGLEEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GlVersionANGLEEntry);
  GPUInfo gpu_info;
  gpu_info.gl_version = "OpenGL ES 3.0 V@66.0 AU@ (CL@)";
  EXPECT_FALSE(entry.Contains(kOsAndroid, "4.4.2", gpu_info));
  gpu_info.gl_version = "3.0 NVIDIA-8.24.11 310.90.9b01";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_version = "OpenGL ES 3.0 (ANGLE 1.2.0.2450)";
  EXPECT_TRUE(entry.Contains(kOsWin, "6.1", gpu_info));
  gpu_info.gl_version = "OpenGL ES 2.0 (ANGLE 1.2.0.2450)";
  EXPECT_FALSE(entry.Contains(kOsWin, "6.1", gpu_info));
}

TEST_F(GpuControlListEntryTest, GlVersionGLEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GlVersionGLEntry);
  GPUInfo gpu_info;
  gpu_info.gl_version = "OpenGL ES 3.0 V@66.0 AU@ (CL@)";
  EXPECT_FALSE(entry.Contains(kOsAndroid, "4.4.2", gpu_info));
  gpu_info.gl_version = "3.0 NVIDIA-8.24.11 310.90.9b01";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_version = "4.0 NVIDIA-8.24.11 310.90.9b01";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_version = "OpenGL ES 3.0 (ANGLE 1.2.0.2450)";
  EXPECT_FALSE(entry.Contains(kOsWin, "6.1", gpu_info));
}

TEST_F(GpuControlListEntryTest, GlVendorEqual) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GlVendorEqual);
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "NVIDIA";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  // Case sensitive.
  gpu_info.gl_vendor = "NVidia";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_vendor = "NVIDIA-x";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
}

TEST_F(GpuControlListEntryTest, GlVendorWithDot) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GlVendorWithDot);
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "X.Org R300 Project";
  EXPECT_TRUE(entry.Contains(kOsLinux, "", gpu_info));
  gpu_info.gl_vendor = "X.Org";
  EXPECT_TRUE(entry.Contains(kOsLinux, "", gpu_info));
}

TEST_F(GpuControlListEntryTest, GlRendererContains) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GlRendererContains);
  GPUInfo gpu_info;
  gpu_info.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  // Case sensitive.
  gpu_info.gl_renderer = "NVIDIA GEFORCE GT 120 OpenGL Engine";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_renderer = "GeForce GT 120 OpenGL Engine";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_renderer = "NVIDIA GeForce";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_renderer = "NVIDIA Ge Force";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
}

TEST_F(GpuControlListEntryTest, GlRendererCaseInsensitive) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_GlRendererCaseInsensitive);
  GPUInfo gpu_info;
  gpu_info.gl_renderer = "software rasterizer";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_renderer = "Software Rasterizer";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
}

TEST_F(GpuControlListEntryTest, GlExtensionsEndWith) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GlExtensionsEndWith);
  GPUInfo gpu_info;
  gpu_info.gl_extensions =
      "GL_SGIS_generate_mipmap "
      "GL_SGIX_shadow "
      "GL_SUN_slice_accum";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gl_extensions =
      "GL_SGIS_generate_mipmap "
      "GL_SUN_slice_accum "
      "GL_SGIX_shadow";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
}

TEST_F(GpuControlListEntryTest, OptimusEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_OptimusEntry);
  EXPECT_EQ(kOsLinux, entry.conditions.os_type);
  GPUInfo gpu_info;
  gpu_info.optimus = true;
  EXPECT_TRUE(entry.Contains(kOsLinux, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, AMDSwitchableEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_AMDSwitchableEntry);
  EXPECT_EQ(kOsMacosx, entry.conditions.os_type);
  GPUInfo gpu_info;
  gpu_info.amd_switchable = true;
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, DriverVendorBeginWith) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_DriverVendorBeginWith);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.driver_vendor = "NVIDIA Corporation";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  // Case sensitive.
  gpu_info.gpu.driver_vendor = "NVidia Corporation";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gpu.driver_vendor = "NVIDIA";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.gpu.driver_vendor = "USA NVIDIA";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
}

TEST_F(GpuControlListEntryTest, LexicalDriverVersionEntry) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_LexicalDriverVersionEntry);
  EXPECT_EQ(kOsLinux, entry.conditions.os_type);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;
  gpu_info.gpu.driver_version = "8.76";
  EXPECT_TRUE(entry.Contains(kOsLinux, "10.6", gpu_info));
  gpu_info.gpu.driver_version = "8.768";
  EXPECT_TRUE(entry.Contains(kOsLinux, "10.6", gpu_info));
  gpu_info.gpu.driver_version = "8.76.8";
  EXPECT_TRUE(entry.Contains(kOsLinux, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, NeedsMoreInfoEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_NeedsMoreInfoEntry);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  EXPECT_TRUE(entry.NeedsMoreInfo(gpu_info, true));
  gpu_info.gpu.driver_version = "10.6";
  EXPECT_FALSE(entry.NeedsMoreInfo(gpu_info, true));
}

TEST_F(GpuControlListEntryTest, NeedsMoreInfoForExceptionsEntry) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_NeedsMoreInfoForExceptionsEntry);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  EXPECT_TRUE(entry.NeedsMoreInfo(gpu_info, true));
  EXPECT_FALSE(entry.NeedsMoreInfo(gpu_info, false));
  gpu_info.gl_renderer = "mesa";
  EXPECT_FALSE(entry.NeedsMoreInfo(gpu_info, true));
}

TEST_F(GpuControlListEntryTest, NeedsMoreInfoForGlVersionEntry) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_NeedsMoreInfoForGlVersionEntry);
  GPUInfo gpu_info;
  EXPECT_TRUE(entry.NeedsMoreInfo(gpu_info, true));
  EXPECT_TRUE(entry.Contains(kOsLinux, std::string(), gpu_info));
  gpu_info.gl_version = "3.1 Mesa 11.1.0";
  EXPECT_FALSE(entry.NeedsMoreInfo(gpu_info, false));
  EXPECT_TRUE(entry.Contains(kOsLinux, std::string(), gpu_info));
  gpu_info.gl_version = "4.1 Mesa 12.1.0";
  EXPECT_FALSE(entry.NeedsMoreInfo(gpu_info, false));
  EXPECT_FALSE(entry.Contains(kOsLinux, std::string(), gpu_info));
  gpu_info.gl_version = "OpenGL ES 2.0 Mesa 12.1.0";
  EXPECT_FALSE(entry.NeedsMoreInfo(gpu_info, false));
  EXPECT_FALSE(entry.Contains(kOsLinux, std::string(), gpu_info));
}

TEST_F(GpuControlListEntryTest, FeatureTypeAllEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_FeatureTypeAllEntry);

  EXPECT_EQ(3u, entry.features.size());
  EXPECT_EQ(1u, CountFeature(entry, TEST_FEATURE_0));
  EXPECT_EQ(1u, CountFeature(entry, TEST_FEATURE_1));
  EXPECT_EQ(1u, CountFeature(entry, TEST_FEATURE_2));
}

TEST_F(GpuControlListEntryTest, FeatureTypeAllEntryWithExceptions) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_FeatureTypeAllEntryWithExceptions);
  EXPECT_EQ(2u, entry.features.size());
  EXPECT_EQ(1u, CountFeature(entry, TEST_FEATURE_1));
  EXPECT_EQ(1u, CountFeature(entry, TEST_FEATURE_2));
}

TEST_F(GpuControlListEntryTest, SingleActiveGPU) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_SingleActiveGPU);
  EXPECT_EQ(kOsMacosx, entry.conditions.os_type);
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.6", gpu_info()));
}

TEST_F(GpuControlListEntryTest, MachineModelName) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_MachineModelName);
  EXPECT_EQ(kOsAndroid, entry.conditions.os_type);
  GPUInfo gpu_info;
  gpu_info.machine_model_name = "Nexus 4";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  gpu_info.machine_model_name = "XT1032";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  gpu_info.machine_model_name = "XT1032i";
  EXPECT_FALSE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  gpu_info.machine_model_name = "Nexus 5";
  EXPECT_FALSE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  gpu_info.machine_model_name = "Nexus";
  EXPECT_FALSE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  gpu_info.machine_model_name = "";
  EXPECT_FALSE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  gpu_info.machine_model_name = "GT-N7100";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  gpu_info.machine_model_name = "GT-I9300";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  gpu_info.machine_model_name = "SCH-I545";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.1", gpu_info));
}

TEST_F(GpuControlListEntryTest, MachineModelNameException) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_MachineModelNameException);
  EXPECT_EQ(kOsAny, entry.conditions.os_type);
  GPUInfo gpu_info;
  gpu_info.machine_model_name = "Nexus 4";
  EXPECT_FALSE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  EXPECT_TRUE(entry.Contains(kOsLinux, "4.1", gpu_info));
  gpu_info.machine_model_name = "Nexus 7";
  EXPECT_FALSE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  EXPECT_TRUE(entry.Contains(kOsLinux, "4.1", gpu_info));
  gpu_info.machine_model_name = "";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.1", gpu_info));
  EXPECT_TRUE(entry.Contains(kOsLinux, "4.1", gpu_info));
}

TEST_F(GpuControlListEntryTest, MachineModelVersion) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_MachineModelVersion);
  GPUInfo gpu_info;
  gpu_info.machine_model_name = "MacBookPro";
  gpu_info.machine_model_version = "7.1";
  EXPECT_EQ(kOsMacosx, entry.conditions.os_type);
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.6", gpu_info));
}

TEST_F(GpuControlListEntryTest, MachineModelVersionException) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_MachineModelVersionException);
  EXPECT_EQ(kOsMacosx, entry.conditions.os_type);
  GPUInfo gpu_info;
  gpu_info.machine_model_name = "MacBookPro";
  gpu_info.machine_model_version = "7.0";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.6", gpu_info));
  gpu_info.machine_model_version = "7.2";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.6", gpu_info));
  gpu_info.machine_model_version = "";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.6", gpu_info));
}

class GpuControlListEntryDualGPUTest : public GpuControlListEntryTest {
 public:
  GpuControlListEntryDualGPUTest() = default;
  ~GpuControlListEntryDualGPUTest() override = default;

  void SetUp() override {
    // Set up a NVIDIA/Intel dual, with NVIDIA as primary and Intel as
    // secondary, and initially Intel is active.
    gpu_info_.gpu.vendor_id = 0x10de;
    gpu_info_.gpu.device_id = 0x0640;
    gpu_info_.gpu.driver_version = "24.21.13.9811";
    gpu_info_.gpu.active = false;
    GPUInfo::GPUDevice second_gpu;
    second_gpu.vendor_id = 0x8086;
    second_gpu.device_id = 0x0166;
    second_gpu.driver_version = "30.0.101.1660";
    second_gpu.active = true;
    gpu_info_.secondary_gpus.push_back(second_gpu);
  }

  void ActivatePrimaryGPU() {
    gpu_info_.gpu.active = true;
    gpu_info_.secondary_gpus[0].active = false;
  }

  void EntryShouldApply(const Entry& entry) const {
    EXPECT_TRUE(EntryApplies(entry));
  }

  void EntryShouldNotApply(const Entry& entry) const {
    EXPECT_FALSE(EntryApplies(entry));
  }

 private:
  bool EntryApplies(const Entry& entry) const {
    EXPECT_EQ(kOsMacosx, entry.conditions.os_type);
    return entry.Contains(kOsMacosx, "10.6", gpu_info());
  }
};

TEST_F(GpuControlListEntryDualGPUTest, CategoryAny) {
  const Entry& entry_intel =
      GetEntry(kGpuControlListEntryDualGPUTest_CategoryAny_Intel);
  EntryShouldApply(entry_intel);
  const Entry& entry_nvidia =
      GetEntry(kGpuControlListEntryDualGPUTest_CategoryAny_NVidia);
  EntryShouldApply(entry_nvidia);
}

TEST_F(GpuControlListEntryDualGPUTest, CategoryPrimarySecondary) {
  const Entry& entry_secondary =
      GetEntry(kGpuControlListEntryDualGPUTest_CategorySecondary);
  EntryShouldApply(entry_secondary);
  const Entry& entry_primary =
      GetEntry(kGpuControlListEntryDualGPUTest_CategoryPrimary);
  EntryShouldNotApply(entry_primary);
  const Entry& entry_default =
      GetEntry(kGpuControlListEntryDualGPUTest_CategoryDefault);
  // Default is active, and the secondary Intel GPU is active.
  EntryShouldApply(entry_default);
}

TEST_F(GpuControlListEntryDualGPUTest, ActiveSecondaryGPU) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryDualGPUTest_ActiveSecondaryGPU);
  // By default, secondary GPU is active.
  EntryShouldApply(entry);
  ActivatePrimaryGPU();
  EntryShouldNotApply(entry);
}

TEST_F(GpuControlListEntryDualGPUTest, VendorOnlyActiveSecondaryGPU) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryDualGPUTest_VendorOnlyActiveSecondaryGPU);
  // By default, secondary GPU is active.
  EntryShouldApply(entry);
  ActivatePrimaryGPU();
  EntryShouldNotApply(entry);
}

TEST_F(GpuControlListEntryDualGPUTest, ActivePrimaryGPU) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryDualGPUTest_ActivePrimaryGPU);
  // By default, secondary GPU is active.
  EntryShouldNotApply(entry);
  ActivatePrimaryGPU();
  EntryShouldApply(entry);
}

TEST_F(GpuControlListEntryDualGPUTest, VendorOnlyActivePrimaryGPU) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryDualGPUTest_VendorOnlyActivePrimaryGPU);
  // By default, secondary GPU is active.
  EntryShouldNotApply(entry);
  ActivatePrimaryGPU();
  EntryShouldApply(entry);
}

TEST_F(GpuControlListEntryDualGPUTest, AnyDriverVersion) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_AnyDriverVersion);
  EntryShouldApply(entry);
  ActivatePrimaryGPU();
  EntryShouldApply(entry);
}

TEST_F(GpuControlListEntryDualGPUTest, ActiveDriverVersion) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_ActiveDriverVersion);
  EntryShouldNotApply(entry);
  ActivatePrimaryGPU();
  EntryShouldApply(entry);
}

TEST_F(GpuControlListEntryTest, PixelShaderVersion) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_PixelShaderVersion);
  EXPECT_EQ(kOsAny, entry.conditions.os_type);
  GPUInfo gpu_info;
  gpu_info.pixel_shader_version = "3.2";
  EXPECT_TRUE(entry.Contains(kOsMacosx, "10.9", gpu_info));
  gpu_info.pixel_shader_version = "4.9";
  EXPECT_FALSE(entry.Contains(kOsMacosx, "10.9", gpu_info));
}

TEST_F(GpuControlListEntryTest, OsVersionZero) {
  {
    const Entry& entry = GetEntry(kGpuControlListEntryTest_OsVersionZeroLT);
    // All forms of version 0 is considered invalid.
    EXPECT_FALSE(entry.Contains(kOsAndroid, "0", gpu_info()));
    EXPECT_FALSE(entry.Contains(kOsAndroid, "0.0", gpu_info()));
    EXPECT_FALSE(entry.Contains(kOsAndroid, "0.00.0", gpu_info()));
  }
  {
    const Entry& entry = GetEntry(kGpuControlListEntryTest_OsVersionZeroAny);
    EXPECT_TRUE(entry.Contains(kOsAndroid, "0", gpu_info()));
    EXPECT_TRUE(entry.Contains(kOsAndroid, "0.0", gpu_info()));
    EXPECT_TRUE(entry.Contains(kOsAndroid, "0.00.0", gpu_info()));
  }
}

TEST_F(GpuControlListEntryTest, OsComparison) {
  {
    const Entry& entry = GetEntry(kGpuControlListEntryTest_OsComparisonAny);
    for (auto os_type : kAllOsType) {
      EXPECT_TRUE(entry.Contains(os_type, std::string(), gpu_info()));
      EXPECT_TRUE(entry.Contains(os_type, "7.8", gpu_info()));
    }
  }
  {
    const Entry& entry = GetEntry(kGpuControlListEntryTest_OsComparisonGE);
    EXPECT_FALSE(entry.Contains(kOsMacosx, "10.8.3", gpu_info()));
    EXPECT_FALSE(entry.Contains(kOsLinux, "10", gpu_info()));
    EXPECT_FALSE(entry.Contains(kOsChromeOS, "13", gpu_info()));
    EXPECT_FALSE(entry.Contains(kOsAndroid, "7", gpu_info()));
    EXPECT_FALSE(entry.Contains(kOsWin, std::string(), gpu_info()));
    EXPECT_TRUE(entry.Contains(kOsWin, "6", gpu_info()));
    EXPECT_TRUE(entry.Contains(kOsWin, "6.1", gpu_info()));
    EXPECT_TRUE(entry.Contains(kOsWin, "7", gpu_info()));
    EXPECT_FALSE(entry.Contains(kOsWin, "5", gpu_info()));
  }
}

TEST_F(GpuControlListEntryTest, ExceptionWithoutVendorId) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_ExceptionWithoutVendorId);
  EXPECT_EQ(0x8086u, entry.exceptions[0].vendor_id);
  EXPECT_EQ(0x8086u, entry.exceptions[1].vendor_id);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.device_id = 0x2a02;
  gpu_info.gpu.driver_version = "9.1";
  EXPECT_FALSE(entry.Contains(kOsLinux, "2.1", gpu_info));
  gpu_info.gpu.driver_version = "9.0";
  EXPECT_TRUE(entry.Contains(kOsLinux, "2.1", gpu_info));
}

TEST_F(GpuControlListEntryTest, MultiGpuStyleAMDSwitchable) {
  GPUInfo gpu_info;
  gpu_info.amd_switchable = true;
  gpu_info.gpu.vendor_id = 0x1002;
  gpu_info.gpu.device_id = 0x6760;
  GPUInfo::GPUDevice integrated_gpu;
  integrated_gpu.vendor_id = 0x8086;
  integrated_gpu.device_id = 0x0116;
  gpu_info.secondary_gpus.push_back(integrated_gpu);

  {  // amd_switchable_discrete entry
    const Entry& entry =
        GetEntry(kGpuControlListEntryTest_MultiGpuStyleAMDSwitchableDiscrete);
    // Integrated GPU is active
    gpu_info.gpu.active = false;
    gpu_info.secondary_gpus[0].active = true;
    EXPECT_FALSE(entry.Contains(kOsWin, "6.0", gpu_info));
    // Discrete GPU is active
    gpu_info.gpu.active = true;
    gpu_info.secondary_gpus[0].active = false;
    EXPECT_TRUE(entry.Contains(kOsWin, "6.0", gpu_info));
  }

  {  // amd_switchable_integrated entry
    const Entry& entry =
        GetEntry(kGpuControlListEntryTest_MultiGpuStyleAMDSwitchableIntegrated);
    // Discrete GPU is active
    gpu_info.gpu.active = true;
    gpu_info.secondary_gpus[0].active = false;
    EXPECT_FALSE(entry.Contains(kOsWin, "6.0", gpu_info));
    // Integrated GPU is active
    gpu_info.gpu.active = false;
    gpu_info.secondary_gpus[0].active = true;
    EXPECT_TRUE(entry.Contains(kOsWin, "6.0", gpu_info));
    // For non AMD switchable
    gpu_info.amd_switchable = false;
    EXPECT_FALSE(entry.Contains(kOsWin, "6.0", gpu_info));
  }
}

TEST_F(GpuControlListEntryTest, InProcessGPU) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_InProcessGPU);
  GPUInfo gpu_info;
  gpu_info.in_process_gpu = true;
  EXPECT_TRUE(entry.Contains(kOsWin, "6.1", gpu_info));
  gpu_info.in_process_gpu = false;
  EXPECT_FALSE(entry.Contains(kOsWin, "6.1", gpu_info));
}

TEST_F(GpuControlListEntryTest, SameGPUTwiceTest) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_SameGPUTwiceTest);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  // Real case on Intel GMA* on Windows
  gpu_info.secondary_gpus.push_back(gpu_info.gpu);
  EXPECT_TRUE(entry.Contains(kOsWin, "6.1", gpu_info));
}

TEST_F(GpuControlListEntryTest, NVidiaNumberingScheme) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_NVidiaNumberingScheme);
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "NVIDIA";
  gpu_info.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0640;
  // test the same driver version number
  gpu_info.gpu.driver_version = "8.17.12.6973";
  EXPECT_TRUE(entry.Contains(kOsWin, "7.0", gpu_info));
  // test a lower driver version number
  gpu_info.gpu.driver_version = "8.15.11.8647";
  EXPECT_TRUE(entry.Contains(kOsWin, "7.0", gpu_info));
  // test a higher driver version number
  gpu_info.gpu.driver_version = "9.18.13.2723";
  EXPECT_FALSE(entry.Contains(kOsWin, "7.0", gpu_info));
}

TEST_F(GpuControlListEntryTest, DirectRendering) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_DirectRendering);
  GPUInfo gpu_info;
  // No info does not match.
  gpu_info.direct_rendering_version = "";
  EXPECT_FALSE(entry.Contains(kOsLinux, "7.0", gpu_info));

  // Indirect rendering does not match.
  gpu_info.direct_rendering_version = "1";
  EXPECT_FALSE(entry.Contains(kOsLinux, "7.0", gpu_info));
  gpu_info.direct_rendering_version = "2";
  EXPECT_TRUE(entry.Contains(kOsLinux, "7.0", gpu_info));
  gpu_info.direct_rendering_version = "2.3";
  EXPECT_TRUE(entry.Contains(kOsLinux, "7.0", gpu_info));
}

TEST_F(GpuControlListEntryTest, GpuSeries) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuSeries);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  // Intel KabyLake
  gpu_info.gpu.device_id = 0x5916;
  EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  // Intel SandyBridge
  gpu_info.gpu.device_id = 0x0116;
  EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  // Intel SkyLake
  gpu_info.gpu.device_id = 0x1916;
  EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  // Non-Intel GPU
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0df8;
  EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
}

TEST_F(GpuControlListEntryTest, GpuSeriesActive) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuSeriesActive);

  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x5916;
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;

  {  // Single GPU
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is primary and active
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.gpu.active = true;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is secondary and active
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    gpu_info.secondary_gpus[0].active = true;
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, NVidia is primary and active
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.gpu.active = true;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, NVidia is secondary and active
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    gpu_info.secondary_gpus[0].active = true;
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }
}

TEST_F(GpuControlListEntryTest, GpuSeriesAny) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuSeriesAny);

  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x5916;
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;

  {  // Single GPU Intel
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Single GPU NVidia
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is primary
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is secondary
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }
}

TEST_F(GpuControlListEntryTest, GpuSeriesPrimary) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuSeriesPrimary);

  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x5916;
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;

  {  // Single GPU
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is primary
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is secondary
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }
}

TEST_F(GpuControlListEntryTest, GpuSeriesSecondary) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuSeriesSecondary);

  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x5916;
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;

  {  // Single GPU
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is primary
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is secondary
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }
}

TEST_F(GpuControlListEntryTest, GpuSeriesInException) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuSeriesInException);

  GPUInfo gpu_info;
  // Intel KabyLake
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.device_id = 0x5916;
  EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  // Intel SandyBridge
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.device_id = 0x0116;
  EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
}

TEST_F(GpuControlListEntryTest, MultipleDrivers) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_MultipleDrivers);
  // The GPUInfo data came from https://crbug.com/810713#c58.
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;
  gpu_info.gpu.device_id = 0x6741;
  gpu_info.gpu.driver_version = "8.951.0.0";
  GPUInfo::GPUDevice intel_device;
  intel_device.vendor_id = 0x8086;
  intel_device.device_id = 0x0116;
  intel_device.driver_version = "8.15.0010.2476";
  gpu_info.secondary_gpus.push_back(intel_device);

  gpu_info.gpu.active = true;
  gpu_info.secondary_gpus[0].active = false;
  EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));

  gpu_info.gpu.active = false;
  gpu_info.secondary_gpus[0].active = true;
  EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
}

TEST_F(GpuControlListEntryTest, GpuGeneration) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuGeneration);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  // Intel SandyBridge
  gpu_info.gpu.device_id = 0x0116;
  EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  // Intel Haswell
  gpu_info.gpu.device_id = 0x0416;
  EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  // Intel Broadwell
  gpu_info.gpu.device_id = 0x1616;
  EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  // Intel KabyLake
  gpu_info.gpu.device_id = 0x5916;
  EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  // Intel IceLake
  gpu_info.gpu.device_id = 0x8A56;
  EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  // Non-Intel GPU
  gpu_info.gpu.vendor_id = 0x10de;
  gpu_info.gpu.device_id = 0x0df8;
  EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
}

TEST_F(GpuControlListEntryTest, GpuGenerationActive) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuGenerationActive);

  // Intel Broadwell
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x1616;
  // NVidia GPU
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;

  {  // Single GPU
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is primary and active
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.gpu.active = true;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is secondary and active
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    gpu_info.secondary_gpus[0].active = true;
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, NVidia is primary and active
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.gpu.active = true;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, NVidia is secondary and active
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    gpu_info.secondary_gpus[0].active = true;
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }
}

TEST_F(GpuControlListEntryTest, GpuGenerationAny) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuGenerationAny);

  // Intel Broadwell
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x1616;
  // NVidia GPU
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;

  {  // Single GPU Intel
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Single GPU Nvidia
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is primary
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is secondary
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }
}

TEST_F(GpuControlListEntryTest, GpuGenerationPrimary) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuGenerationPrimary);

  // Intel Broadwell
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x1616;
  // NVidia GPU
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;

  {  // Single GPU
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is primary
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is secondary
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }
}

TEST_F(GpuControlListEntryTest, GpuGenerationSecondary) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_GpuGenerationSecondary);

  // Intel Broadwell
  GPUInfo::GPUDevice intel_gpu;
  intel_gpu.vendor_id = 0x8086;
  intel_gpu.device_id = 0x1616;
  // NVidia GPU
  GPUInfo::GPUDevice nvidia_gpu;
  nvidia_gpu.vendor_id = 0x10de;
  nvidia_gpu.device_id = 0x0df8;


  {  // Single GPU
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is primary
    GPUInfo gpu_info;
    gpu_info.gpu = intel_gpu;
    gpu_info.secondary_gpus.push_back(nvidia_gpu);
    EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));
  }

  {  // Dual GPU, Intel is secondary
    GPUInfo gpu_info;
    gpu_info.gpu = nvidia_gpu;
    gpu_info.secondary_gpus.push_back(intel_gpu);
    EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
  }
}

#if BUILDFLAG(IS_WIN)
TEST_F(GpuControlListEntryTest, HardwareOverlay) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_HardwareOverlay);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.overlay_info.supports_overlays = true;
  EXPECT_FALSE(entry.Contains(kOsWin, "10.0", gpu_info));

  gpu_info.overlay_info.supports_overlays = false;
  EXPECT_TRUE(entry.Contains(kOsWin, "10.0", gpu_info));
}
#endif  // BUILDFLAG(IS_WIN)

TEST_F(GpuControlListEntryTest, TestSubpixelFontRendering) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_SubpixelFontRendering);

  GPUInfo gpu_info;
  gpu_info.subpixel_font_rendering = true;
  gpu_info.gl_renderer = "Mali0xx";

  EXPECT_TRUE(entry.Contains(kOsChromeOS, "10.0", gpu_info));

  gpu_info.subpixel_font_rendering = false;
  gpu_info.gl_renderer = "Mali1xx";
  EXPECT_FALSE(entry.Contains(kOsChromeOS, "10.0", gpu_info));

  gpu_info.subpixel_font_rendering = false;
  gpu_info.gl_renderer = "DontCare";
  EXPECT_FALSE(entry.Contains(kOsChromeOS, "10.0", gpu_info));

  gpu_info.subpixel_font_rendering = true;
  gpu_info.gl_renderer = "DontCare";
  EXPECT_FALSE(entry.Contains(kOsChromeOS, "10.0", gpu_info));

  gpu_info.subpixel_font_rendering = false;
  gpu_info.gl_renderer = "Supported";
  EXPECT_TRUE(entry.Contains(kOsChromeOS, "10.0", gpu_info));

  gpu_info.subpixel_font_rendering = true;
  gpu_info.gl_renderer = "Supported";
  EXPECT_FALSE(entry.Contains(kOsChromeOS, "10.0", gpu_info));

  gpu_info.subpixel_font_rendering = true;
  gpu_info.gl_renderer = "Others";
  EXPECT_TRUE(entry.Contains(kOsChromeOS, "10.0", gpu_info));

  // Not ChromeOS
  EXPECT_FALSE(entry.Contains(kOsLinux, "10.0", gpu_info));
}

TEST_F(GpuControlListEntryTest, TestSubpixelFontRenderingDontCare) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_SubpixelFontRenderingDontCare);

  GPUInfo gpu_info;
  gpu_info.subpixel_font_rendering = true;
  gpu_info.gl_renderer = "Mali0xx";

  EXPECT_TRUE(entry.Contains(kOsChromeOS, "10.0", gpu_info));

  gpu_info.subpixel_font_rendering = false;
  EXPECT_TRUE(entry.Contains(kOsChromeOS, "10.0", gpu_info));
}

TEST_F(GpuControlListEntryTest, IntelDriverVendorEntry) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_IntelDriverVendorEntry);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.driver_vendor = "Intel(R) UHD Graphics 630";
  gpu_info.gpu.driver_version = "25.20.100.5000";
  EXPECT_FALSE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.driver_version = "23.20.100.6500";
  EXPECT_TRUE(entry.Contains(kOsWin, "", gpu_info));
}

TEST_F(GpuControlListEntryTest, IntelDriverVersionEntry) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_IntelDriverVersionEntry);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x8086;
  gpu_info.gpu.driver_version = "23.20.100.8000";
  EXPECT_FALSE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.driver_version = "25.20.100.6000";
  EXPECT_TRUE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.driver_version = "24.20.99.6000";
  EXPECT_TRUE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.driver_version = "24.20.101.6000";
  EXPECT_FALSE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.driver_version = "25.20.100.7000";
  EXPECT_TRUE(entry.Contains(kOsWin, "", gpu_info));
}

TEST_F(GpuControlListEntryTest, NativeAngleRenderer) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_NativeAngleRenderer);
  GPUInfo gpu_info;
  gpu_info.gl_renderer =
      "ANGLE (Samsung Electronics Co. Ltd., "
      "ANGLE (Samsung Xclipse 920) on Vulkan 1.1.179, "
      "OpenGL ES 3.2 ANGLE git hash: 41a335098084)";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.4.2", gpu_info));

  gpu_info.gl_renderer = "ANGLE (Samsung Xclipse 920) on Vulkan 1.1.179";
  EXPECT_TRUE(entry.Contains(kOsAndroid, "4.4.2", gpu_info));
}

#if BUILDFLAG(IS_WIN)
TEST_F(GpuControlListEntryTest, DeviceRevisionEntry) {
  const Entry& entry = GetEntry(kGpuControlListEntryTest_DeviceRevisionEntry);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;
  gpu_info.gpu.device_id = 0x15DD;
  gpu_info.gpu.revision = 0x86;
  gpu_info.gpu.driver_version = "26.20.12055.1000";
  EXPECT_TRUE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.driver_version = "26.20.15023.6032";
  EXPECT_FALSE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.device_id = 0x15D8;
  gpu_info.gpu.revision = 0xE1;
  gpu_info.gpu.driver_version = "26.20.12055.1000";
  EXPECT_FALSE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.revision = 0xE3;
  EXPECT_TRUE(entry.Contains(kOsWin, "", gpu_info));
}

TEST_F(GpuControlListEntryTest, DeviceRevisionUnspecifiedEntry) {
  const Entry& entry =
      GetEntry(kGpuControlListEntryTest_DeviceRevisionUnspecifiedEntry);
  GPUInfo gpu_info;
  gpu_info.gpu.vendor_id = 0x1002;
  gpu_info.gpu.device_id = 0x15DD;
  gpu_info.gpu.revision = 0x86;
  EXPECT_TRUE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.revision = 0x91;
  EXPECT_TRUE(entry.Contains(kOsWin, "", gpu_info));
  gpu_info.gpu.revision = 0x0;
  EXPECT_TRUE(entry.Contains(kOsWin, "", gpu_info));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace gpu
