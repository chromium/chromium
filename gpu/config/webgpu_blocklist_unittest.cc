// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/webgpu_blocklist.h"

#include "build/build_config.h"
#include "gpu/config/webgpu_blocklist_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/dawn/include/dawn/webgpu.h"
#include "ui/gl/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace gpu {

bool IsWebGPUAdapterBlocklisted(const WGPUAdapterInfo& info,
                                const char* blocklist_string = "") {
  return detail::GetWebGPUAdapterBlocklistReason(
             *reinterpret_cast<const wgpu::AdapterInfo*>(&info),
             {
                 .blocklist_string = blocklist_string,
             }) != WebGPUBlocklistReason::None;
}

class WebGPUBlocklistTest : public testing::Test {};

#if BUILDFLAG(IS_ANDROID)
// Android is currently more restrictive than other platforms around which GPUs
// are allowed, which causes the usual tests to fail. This test exercises the
// Android-specific restrictions.

TEST_F(WebGPUBlocklistTest, BlockAndroidVendorId) {
  const auto* build_info = base::android::BuildInfo::GetInstance();

  WGPUAdapterInfo info1 = {};
  info1.vendorID = 0x13B5;

  WGPUAdapterInfo info2 = {};
  info2.vendorID = 0x5143;

  WGPUAdapterInfo info3 = {};
  info3.vendorID = 0x8086;

  if (build_info->sdk_int() < base::android::SDK_VERSION_S) {
    // If the Android version is R or lower everything should be blocked.
    EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1));
    EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2));
    EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info3));
    return;
  }

  // Test the default vendor blocks
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info1));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info2));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info3));

  // Test that blocking a vendor which is otherwise allowed still works
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "13b5"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info2, "13b5"));

  // Test blocking *
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "*"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "*"));

  // Test blocking a list of patterns
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "13b5|5143"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "13b5|5143"));
}

#else
TEST_F(WebGPUBlocklistTest, BlockVendorId) {
  WGPUAdapterInfo info1 = {};
  info1.vendorID = 0x8086;

  WGPUAdapterInfo info2 = {};
  info2.vendorID = 0x1002;

  WGPUAdapterInfo info3 = {};
  info3.vendorID = 0x0042;

  // Test blocking exactly a vendorID
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "8086"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "1002"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info3, "42"));

  // Test blocking a mismatching vendorID
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info1, "1002"));

  // Test blocking *
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "*"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "*"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info3, "*"));

  // Test blocking a list of patterns
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "8086|1002"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "8086|1002"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info3, "8086|1002"));
}

TEST_F(WebGPUBlocklistTest, BlockDeviceIdOrArch) {
  WGPUAdapterInfo info1 = {};
  info1.vendorID = 0x8086;
  info1.deviceID = 0x1;
#if defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info1.architecture = {"gen-9", WGPU_STRLEN};
#else   // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info1.architecture = "gen-9";
#endif  // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)

  WGPUAdapterInfo info2 = {};
  info2.vendorID = 0x8086;
  info2.deviceID = 0x2;
#if defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info2.architecture = {"gen-9", WGPU_STRLEN};
#else   // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info2.architecture = "gen-9";
#endif  // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)

  WGPUAdapterInfo info3 = {};
  info3.vendorID = 0x1002;
  info3.deviceID = 0x1;
#if defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info3.architecture = {"gcn-3", WGPU_STRLEN};
#else   // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info3.architecture = "gcn-3";
#endif  // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)

  // Test blocking exactly a vendor and deviceID
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "8086:1"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "8086:2"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info3, "1002:1"));

  // Test blocking exactly a vendor and architecture
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "8086:gen-9"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "8086:gen-9"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info3, "1002:gcn-3"));

  // Test blocking a mismatching deviceID
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info1, "8086:2"));

  // Test blocking a mismatching architecture
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info1, "8086:gen-8"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info3, "1002:gcn-4"));

  // Test blocking a mismatching vendor id, with matching architecture
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info1, "1002:gen-9"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info3, "8086:gcn-3"));

  // Test blocking *
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "8086:*"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "8086:*"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info3, "8086:*"));

  // Test blocking a list of deviceIDs
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "8086:1|8086:2"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "8086:1|8086:2"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(info3, "8086:1|8086:2"));

  // Test blocking any vendor ID with a list of architectures
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info1, "*:gen-9|*:gcn-3"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info2, "*:gen-9|*:gcn-3"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(info3, "*:gen-9|*:gcn-3"));
}

TEST_F(WebGPUBlocklistTest, BlockDriverDescription) {
  WGPUAdapterInfo info1 = {};
  info1.vendorID = 0x8086;
  info1.deviceID = 0x1;
#if defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info1.architecture = {"gen-9", WGPU_STRLEN};
  info1.description = {"D3D12 driver version 31.0.101.2111", WGPU_STRLEN};
#else   // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info1.architecture = "gen-9";
  info1.description = "D3D12 driver version 31.0.101.2111";
#endif  // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)

  WGPUAdapterInfo info2 = {};
  info2.vendorID = 0x8086;
  info2.deviceID = 0x1;
#if defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info2.architecture = {"gen-9", WGPU_STRLEN};
  info2.description = {"D3D12 driver version 33.0.100.0004", WGPU_STRLEN};
#else   // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info2.architecture = "gen-9";
  info2.description = "D3D12 driver version 33.0.100.0004";
#endif  // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)

  WGPUAdapterInfo info3 = {};
  info3.vendorID = 0x1002;
  info3.deviceID = 0x1;
#if defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info3.architecture = {"gcn-3", WGPU_STRLEN};
  info3.description = {"D3D12 driver version 31.0.203.3113", WGPU_STRLEN};
#else   // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)
  info3.architecture = "gcn-3";
  info3.description = "D3D12 driver version 31.0.203.3113";
#endif  // defined(WGPU_BREAKING_CHANGE_STRING_VIEW_OUTPUT_STRUCTS)

  // Test blocking specific driver versions, with vendor+device ids
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      info1, "8086:1:D3D12 driver version 31.0.101.2111"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      info2, "8086:1:D3D12 driver version 33.0.100.0004"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      info3, "1002:1:D3D12 driver version 31.0.203.3113"));

  // Test blocking specific driver versions, regardless of vendor+device ids
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      info1, "*:*:D3D12 driver version 31.0.101.2111"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      info2, "*:*:D3D12 driver version 33.0.100.0004"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      info3, "*:*:D3D12 driver version 31.0.203.3113"));

  // Test blocking specific driver version patterns
  EXPECT_TRUE(
      IsWebGPUAdapterBlocklisted(info1, "*:*:D3D12 driver version 3*.0.1*"));
  EXPECT_TRUE(
      IsWebGPUAdapterBlocklisted(info2, "*:*:D3D12 driver version 3*.0.1*"));
  EXPECT_FALSE(
      IsWebGPUAdapterBlocklisted(info3, "*:*:D3D12 driver version 3*.0.1*"));

  EXPECT_TRUE(
      IsWebGPUAdapterBlocklisted(info1, "*:*:D3D12 driver version 31.*"));
  EXPECT_FALSE(
      IsWebGPUAdapterBlocklisted(info2, "*:*:D3D12 driver version 31.*"));
  EXPECT_TRUE(
      IsWebGPUAdapterBlocklisted(info3, "*:*:D3D12 driver version 31.*"));
}
#endif  // BUILDFLAG(IS_ANDROID) else

}  // namespace gpu
