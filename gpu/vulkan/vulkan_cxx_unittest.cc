// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_cxx.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

class VulkanCXXTest : public testing::Test {
 public:
  VulkanCXXTest() = default;
  ~VulkanCXXTest() override = default;

  void SetUp() override {
    use_swiftshader_ =
        base::CommandLine::ForCurrentProcess()->HasSwitch("use-swiftshader");
    base::FilePath path;
#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_CHROMEOS) || \
    defined(OS_FUCHSIA)
    if (use_swiftshader_) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
      EXPECT_TRUE(base::PathService::Get(base::DIR_MODULE, &path));
      path = path.Append("libvk_swiftshader.so");
#else
      return;
#endif
    } else {
      path = base::FilePath("libvulkan.so.1");
    }
#elif defined(OS_WIN)
    if (use_swiftshader_) {
      EXPECT_TRUE(base::PathService::Get(base::DIR_MODULE, &path));
      path = path.Append(L"vk_swiftshader.dll");
    } else {
      path = base::FilePath(L"vulkan-1.dll");
    }
#else
#error "Not supported platform"
#endif

    auto* vulkan_function_pointers = GetVulkanFunctionPointers();
    base::NativeLibraryLoadError native_library_load_error;
    vulkan_function_pointers->vulkan_loader_library =
        base::LoadNativeLibrary(path, &native_library_load_error);
    EXPECT_TRUE(vulkan_function_pointers->vulkan_loader_library);
  }

  void TearDown() override {
    auto* vulkan_function_pointers = GetVulkanFunctionPointers();
    base::UnloadNativeLibrary(vulkan_function_pointers->vulkan_loader_library);
  }

 private:
  bool use_swiftshader_ = false;
};

TEST_F(VulkanCXXTest, CreateInstanceUnique) {
  auto* vulkan_function_pointers = GetVulkanFunctionPointers();
  EXPECT_TRUE(vulkan_function_pointers->BindUnassociatedFunctionPointers());

  vk::Result result;
  uint32_t api_version;
  std::tie(result, api_version) = vk::enumerateInstanceVersion();
  EXPECT_EQ(result, vk::Result::eSuccess);
  EXPECT_GE(api_version, kVulkanRequiredApiVersion);

  vk::ApplicationInfo app_info("VulkanCXXTest", 0, nullptr, 0,
                               kVulkanRequiredApiVersion);
  vk::InstanceCreateInfo instance_create_info({}, &app_info);
  auto result_value = vk::createInstanceUnique(instance_create_info);
  EXPECT_EQ(result_value.result, vk::Result::eSuccess);

  vk::UniqueInstance instance = std::move(result_value.value);
  EXPECT_TRUE(instance);

  EXPECT_TRUE(vulkan_function_pointers->BindInstanceFunctionPointers(
      instance.get(), kVulkanRequiredApiVersion, gfx::ExtensionSet()));

  instance.reset();
}

}  // namespace gpu
