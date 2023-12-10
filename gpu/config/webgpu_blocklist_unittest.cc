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

class WebGPUBlocklistTest : public testing::Test {};

#if BUILDFLAG(IS_ANDROID)
// Android is currently more restrictive than other platforms around which GPUs
// are allowed, which causes the usual tests to fail. This test exercises the
// Android-specific restrictions.

TEST_F(WebGPUBlocklistTest, BlockAndroidVendorId) {
  const auto* build_info = base::android::BuildInfo::GetInstance();

  WGPUAdapterProperties properties1 = {};
  properties1.vendorID = 0x13B5;

  WGPUAdapterProperties properties2 = {};
  properties2.vendorID = 0x5143;

  WGPUAdapterProperties properties3 = {};
  properties3.vendorID = 0x8086;

  if (build_info->sdk_int() < base::android::SDK_VERSION_S) {
    // If the Android version is R or lower everything should be blocked.
    EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1));
    EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2));
    EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties3));
    return;
  }

  // Test the default vendor blocks
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties1));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties2));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties3));

  // Test that blocking a vendor which is otherwise allowed still works
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "13b5"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties2, "13b5"));

  // Test blocking *
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "*"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "*"));

  // Test blocking a list of patterns
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "13b5|5143"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "13b5|5143"));
}

#else
TEST_F(WebGPUBlocklistTest, BlockVendorId) {
  WGPUAdapterProperties properties1 = {};
  properties1.vendorID = 0x8086;

  WGPUAdapterProperties properties2 = {};
  properties2.vendorID = 0x1002;

  WGPUAdapterProperties properties3 = {};
  properties3.vendorID = 0x0042;

  // Test blocking exactly a vendorID
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "8086"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "1002"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties3, "42"));

  // Test blocking a mismatching vendorID
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties1, "1002"));

  // Test blocking *
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "*"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "*"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties3, "*"));

  // Test blocking a list of patterns
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "8086|1002"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "8086|1002"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties3, "8086|1002"));
}

TEST_F(WebGPUBlocklistTest, BlockDeviceIdOrArch) {
  WGPUAdapterProperties properties1 = {};
  properties1.vendorID = 0x8086;
  properties1.deviceID = 0x1;
  properties1.architecture = "gen-9";

  WGPUAdapterProperties properties2 = {};
  properties2.vendorID = 0x8086;
  properties2.deviceID = 0x2;
  properties2.architecture = "gen-9";

  WGPUAdapterProperties properties3 = {};
  properties3.vendorID = 0x1002;
  properties3.deviceID = 0x1;
  properties3.architecture = "gcn-3";

  // Test blocking exactly a vendor and deviceID
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "8086:1"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "8086:2"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties3, "1002:1"));

  // Test blocking exactly a vendor and architecture
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "8086:gen-9"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "8086:gen-9"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties3, "1002:gcn-3"));

  // Test blocking a mismatching deviceID
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties1, "8086:2"));

  // Test blocking a mismatching architecture
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties1, "8086:gen-8"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties3, "1002:gcn-4"));

  // Test blocking a mismatching vendor id, with matching architecture
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties1, "1002:gen-9"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties3, "8086:gcn-3"));

  // Test blocking *
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "8086:*"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "8086:*"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties3, "8086:*"));

  // Test blocking a list of deviceIDs
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "8086:1|8086:2"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "8086:1|8086:2"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties3, "8086:1|8086:2"));

  // Test blocking any vendor ID with a list of architectures
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1, "*:gen-9|*:gcn-3"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2, "*:gen-9|*:gcn-3"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties3, "*:gen-9|*:gcn-3"));
}

TEST_F(WebGPUBlocklistTest, BlockDriverDescription) {
  WGPUAdapterProperties properties1 = {};
  properties1.vendorID = 0x8086;
  properties1.deviceID = 0x1;
  properties1.architecture = "gen-9";
  properties1.driverDescription = "D3D12 driver version 31.0.101.2111";

  WGPUAdapterProperties properties2 = {};
  properties2.vendorID = 0x8086;
  properties2.deviceID = 0x1;
  properties2.architecture = "gen-9";
  properties2.driverDescription = "D3D12 driver version 33.0.100.0004";

  WGPUAdapterProperties properties3 = {};
  properties3.vendorID = 0x1002;
  properties3.deviceID = 0x1;
  properties3.architecture = "gcn-3";
  properties3.driverDescription = "D3D12 driver version 31.0.203.3113";

  // Test blocking specific driver versions, with vendor+device ids
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      properties1, "8086:1:D3D12 driver version 31.0.101.2111"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      properties2, "8086:1:D3D12 driver version 33.0.100.0004"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      properties3, "1002:1:D3D12 driver version 31.0.203.3113"));

  // Test blocking specific driver versions, regardless of vendor+device ids
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      properties1, "*:*:D3D12 driver version 31.0.101.2111"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      properties2, "*:*:D3D12 driver version 33.0.100.0004"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(
      properties3, "*:*:D3D12 driver version 31.0.203.3113"));

  // Test blocking specific driver version patterns
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties1,
                                         "*:*:D3D12 driver version 3*.0.1*"));
  EXPECT_TRUE(IsWebGPUAdapterBlocklisted(properties2,
                                         "*:*:D3D12 driver version 3*.0.1*"));
  EXPECT_FALSE(IsWebGPUAdapterBlocklisted(properties3,
                                          "*:*:D3D12 driver version 3*.0.1*"));

  EXPECT_TRUE(
      IsWebGPUAdapterBlocklisted(properties1, "*:*:D3D12 driver version 31.*"));
  EXPECT_FALSE(
      IsWebGPUAdapterBlocklisted(properties2, "*:*:D3D12 driver version 31.*"));
  EXPECT_TRUE(
      IsWebGPUAdapterBlocklisted(properties3, "*:*:D3D12 driver version 31.*"));
}
#endif  // BUILDFLAG(IS_ANDROID) else

}  // namespace gpu
