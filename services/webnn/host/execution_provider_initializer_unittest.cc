// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/host/execution_provider_initializer.h"

#include <windows.h>

#include <appmodel.h>

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "services/webnn/public/cpp/execution_providers_info.h"
#include "services/webnn/public/mojom/ep_package_info.mojom.h"
#include "services/webnn/webnn_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webnn {

namespace {

using EpPackageInfoMap = base::flat_map<std::string, mojom::EpPackageInfoPtr>;

constexpr wchar_t kOpenVINOLibraryPath[] = L"C:\\ov\\openvino_plugin.dll";
constexpr wchar_t kWebGpuLibraryPath[] = L"C:\\webgpu\\webgpu_plugin.dll";

EpPackageInfoMap AddTestingEps(const std::wstring& switch_value,
                               EpPackageInfoMap map = {}) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchNative(switches::kWebNNOrtEpLibraryPathForTesting,
                                  switch_value);
  AddTestingEpsFromCommandLine(command_line, map);
  return map;
}

void ExpectTestingEp(const EpPackageInfoMap& map,
                     std::string_view ep_name,
                     const wchar_t* library_path) {
  auto it = map.find(ep_name);
  ASSERT_NE(it, map.end());
  EXPECT_TRUE(it->second->family_name.empty());
  EXPECT_EQ(it->second->library_path, base::FilePath(library_path));
}

}  // namespace

TEST(ExecutionProviderInitializerTest, AbsentSwitch) {
  EpPackageInfoMap map;
  AddTestingEpsFromCommandLine(base::CommandLine(base::CommandLine::NO_PROGRAM),
                               map);
  EXPECT_TRUE(map.empty());
}

TEST(ExecutionProviderInitializerTest, OnePair) {
  EpPackageInfoMap map = AddTestingEps(
      std::wstring(L"OpenVINOExecutionProvider?") + kOpenVINOLibraryPath);
  EXPECT_EQ(map.size(), 1u);
  ExpectTestingEp(map, kOpenVINOExecutionProvider, kOpenVINOLibraryPath);
}

TEST(ExecutionProviderInitializerTest, TwoDifferentEps) {
  EpPackageInfoMap map = AddTestingEps(
      std::wstring(L"OpenVINOExecutionProvider?") + kOpenVINOLibraryPath +
      L",WebGpuExecutionProvider?" + kWebGpuLibraryPath);
  EXPECT_EQ(map.size(), 2u);
  ExpectTestingEp(map, kOpenVINOExecutionProvider, kOpenVINOLibraryPath);
  ExpectTestingEp(map, kWebGpuExecutionProvider, kWebGpuLibraryPath);
}

TEST(ExecutionProviderInitializerTest, OddNumberOfParts) {
  EXPECT_TRUE(AddTestingEps(std::wstring(L"OpenVINOExecutionProvider?") +
                            kOpenVINOLibraryPath + L",WebGpuExecutionProvider")
                  .empty());
}

TEST(ExecutionProviderInitializerTest, BlankEntry) {
  EXPECT_TRUE(AddTestingEps(std::wstring(L"OpenVINOExecutionProvider?") +
                            kOpenVINOLibraryPath + L",")
                  .empty());
}

TEST(ExecutionProviderInitializerTest, UnknownEpNameInSecondPair) {
  EXPECT_TRUE(AddTestingEps(std::wstring(L"OpenVINOExecutionProvider?") +
                            kOpenVINOLibraryPath + L",UnknownProvider?" +
                            kWebGpuLibraryPath)
                  .empty());
}

TEST(ExecutionProviderInitializerTest, DuplicatedEpName) {
  EXPECT_TRUE(AddTestingEps(std::wstring(L"OpenVINOExecutionProvider?") +
                            kOpenVINOLibraryPath +
                            L",OpenVINOExecutionProvider?" + kWebGpuLibraryPath)
                  .empty());
}

TEST(ExecutionProviderInitializerTest, OverridesRealEp) {
  EpPackageInfoMap real_eps;
  real_eps.emplace(std::string(kOpenVINOExecutionProvider),
                   mojom::EpPackageInfo::New(
                       /*family_name=*/L"OpenVINOFamily",
                       /*version=*/PACKAGE_VERSION{},
                       /*library_path=*/base::FilePath(L"C:\\real.dll")));

  EpPackageInfoMap map = AddTestingEps(
      std::wstring(L"OpenVINOExecutionProvider?") + kOpenVINOLibraryPath,
      std::move(real_eps));
  EXPECT_EQ(map.size(), 1u);
  ExpectTestingEp(map, kOpenVINOExecutionProvider, kOpenVINOLibraryPath);
}

}  // namespace webnn
