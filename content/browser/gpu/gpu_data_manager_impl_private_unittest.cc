// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/gpu/gpu_data_manager_impl_private.h"
#include "content/browser/gpu/gpu_data_manager_testing_autogen.h"
#include "content/browser/gpu/gpu_data_manager_testing_entry_enums_autogen.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/memory_stats.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMECAST)
#include "chromecast/chromecast_buildflags.h"
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
#define CAST_AUDIO_ONLY
#endif
#endif

namespace content {
namespace {

class TestObserver : public GpuDataManagerObserver {
 public:
  TestObserver() {}
  ~TestObserver() override {}

  bool gpu_info_updated() const { return gpu_info_updated_; }

  void OnGpuInfoUpdate() override { gpu_info_updated_ = true; }

  void Reset() {
    gpu_info_updated_ = false;
  }

 private:
  bool gpu_info_updated_ = false;
};

static base::Time GetTimeForTesting() {
  return base::Time::FromDoubleT(1000);
}

static GURL GetDomain1ForTesting() {
  return GURL("http://foo.com/");
}

static GURL GetDomain2ForTesting() {
  return GURL("http://bar.com/");
}

gpu::GpuFeatureInfo ALLOW_UNUSED_TYPE
GetGpuFeatureInfoWithOneDisabled(gpu::GpuFeatureType disabled_feature) {
  gpu::GpuFeatureInfo gpu_feature_info;
  for (auto& status : gpu_feature_info.status_values)
    status = gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled;
  gpu_feature_info.status_values[disabled_feature] =
      gpu::GpuFeatureStatus::kGpuFeatureStatusDisabled;
  return gpu_feature_info;
}

}  // namespace

class GpuDataManagerImplPrivateTest : public testing::Test {
 public:
  GpuDataManagerImplPrivateTest() {}
  ~GpuDataManagerImplPrivateTest() override {}

 protected:
  // scoped_ptr doesn't work with GpuDataManagerImpl because its
  // destructor is private. GpuDataManagerImplPrivateTest is however a friend
  // so we can make a little helper class here.
  class ScopedGpuDataManagerImpl {
   public:
    ScopedGpuDataManagerImpl() { EXPECT_TRUE(impl_.private_.get()); }
    ~ScopedGpuDataManagerImpl() = default;

    GpuDataManagerImpl* get() { return &impl_; }
    GpuDataManagerImpl* operator->() { return &impl_; }

   private:
    GpuDataManagerImpl impl_;
    DISALLOW_COPY_AND_ASSIGN(ScopedGpuDataManagerImpl);
  };

  // We want to test the code path where GpuDataManagerImplPrivate is created
  // in the GpuDataManagerImpl constructor.
  class ScopedGpuDataManagerImplPrivate {
   public:
    ScopedGpuDataManagerImplPrivate() { EXPECT_TRUE(impl_.private_.get()); }
    ~ScopedGpuDataManagerImplPrivate() = default;

    // NO_THREAD_SAFETY_ANALYSIS should be fine below, because unit tests
    // pinky-promise to only run single-threaded.
    GpuDataManagerImplPrivate* get() NO_THREAD_SAFETY_ANALYSIS {
      return impl_.private_.get();
    }
    GpuDataManagerImplPrivate* operator->() NO_THREAD_SAFETY_ANALYSIS {
      return impl_.private_.get();
    }

   private:
    GpuDataManagerImpl impl_;
    DISALLOW_COPY_AND_ASSIGN(ScopedGpuDataManagerImplPrivate);
  };

  base::Time JustBeforeExpiration(const GpuDataManagerImplPrivate* manager);
  base::Time JustAfterExpiration(const GpuDataManagerImplPrivate* manager);
  void TestBlockingDomainFrom3DAPIs(gpu::DomainGuilt guilt_level);
  void TestUnblockingDomainFrom3DAPIs(gpu::DomainGuilt guilt_level);

  base::test::SingleThreadTaskEnvironment task_environment_;
};

// We use new method instead of GetInstance() method because we want
// each test to be independent of each other.

TEST_F(GpuDataManagerImplPrivateTest, GpuInfoUpdate) {
  ScopedGpuDataManagerImpl manager;

  TestObserver observer;
  manager->AddObserver(&observer);

  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_FALSE(observer.gpu_info_updated());

  gpu::GPUInfo gpu_info;
  manager->UpdateGpuInfo(gpu_info, absl::nullopt);
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(observer.gpu_info_updated());
}

base::Time GpuDataManagerImplPrivateTest::JustBeforeExpiration(
    const GpuDataManagerImplPrivate* manager) {
  return GetTimeForTesting() + base::TimeDelta::FromMilliseconds(
      manager->GetBlockAllDomainsDurationInMs()) -
      base::TimeDelta::FromMilliseconds(3);
}

base::Time GpuDataManagerImplPrivateTest::JustAfterExpiration(
    const GpuDataManagerImplPrivate* manager) {
  return GetTimeForTesting() + base::TimeDelta::FromMilliseconds(
      manager->GetBlockAllDomainsDurationInMs()) +
      base::TimeDelta::FromMilliseconds(3);
}

void GpuDataManagerImplPrivateTest::TestBlockingDomainFrom3DAPIs(
    gpu::DomainGuilt guilt_level) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                      guilt_level,
                                      GetTimeForTesting());

  // This domain should be blocked no matter what.
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustAfterExpiration(manager.get())));
}

void GpuDataManagerImplPrivateTest::TestUnblockingDomainFrom3DAPIs(
    gpu::DomainGuilt guilt_level) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(GetDomain1ForTesting(),
                                       guilt_level,
                                       GetTimeForTesting());

  // Unblocking the domain should work.
  manager->UnblockDomainFrom3DAPIs(GetDomain1ForTesting());
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustAfterExpiration(manager.get())));
}

TEST_F(GpuDataManagerImplPrivateTest, BlockGuiltyDomainFrom3DAPIs) {
  TestBlockingDomainFrom3DAPIs(gpu::DomainGuilt::kKnown);
}

TEST_F(GpuDataManagerImplPrivateTest, BlockDomainOfUnknownGuiltFrom3DAPIs) {
  TestBlockingDomainFrom3DAPIs(gpu::DomainGuilt::kUnknown);
}

TEST_F(GpuDataManagerImplPrivateTest, BlockAllDomainsFrom3DAPIs) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(
      GetDomain1ForTesting(), gpu::DomainGuilt::kUnknown, GetTimeForTesting());

  // Blocking of other domains should expire.
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustAfterExpiration(manager.get())));
}

TEST_F(GpuDataManagerImplPrivateTest, UnblockGuiltyDomainFrom3DAPIs) {
  TestUnblockingDomainFrom3DAPIs(gpu::DomainGuilt::kKnown);
}

TEST_F(GpuDataManagerImplPrivateTest, UnblockDomainOfUnknownGuiltFrom3DAPIs) {
  TestUnblockingDomainFrom3DAPIs(gpu::DomainGuilt::kUnknown);
}

TEST_F(GpuDataManagerImplPrivateTest, UnblockOtherDomainFrom3DAPIs) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(
      GetDomain1ForTesting(), gpu::DomainGuilt::kUnknown, GetTimeForTesting());

  manager->UnblockDomainFrom3DAPIs(GetDomain2ForTesting());

  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustBeforeExpiration(manager.get())));

  // The original domain should still be blocked.
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustBeforeExpiration(manager.get())));
}

TEST_F(GpuDataManagerImplPrivateTest, UnblockThisDomainFrom3DAPIs) {
  ScopedGpuDataManagerImplPrivate manager;

  manager->BlockDomainFrom3DAPIsAtTime(
      GetDomain1ForTesting(), gpu::DomainGuilt::kUnknown, GetTimeForTesting());

  manager->UnblockDomainFrom3DAPIs(GetDomain1ForTesting());

  // This behavior is debatable. Perhaps the GPU reset caused by
  // domain 1 should still cause other domains to be blocked.
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustBeforeExpiration(manager.get())));
}

// Android and Chrome OS do not support software compositing, while Fuchsia does
// not support falling back to software from Vulkan.
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#if !defined(OS_FUCHSIA)
TEST_F(GpuDataManagerImplPrivateTest, FallbackToSwiftShader) {
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::SWIFTSHADER, manager->GetGpuMode());
}

TEST_F(GpuDataManagerImplPrivateTest, FallbackWithSwiftShaderDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableSoftwareRasterizer);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  gpu::GpuMode expected_mode = gpu::GpuMode::DISPLAY_COMPOSITOR;
  EXPECT_EQ(expected_mode, manager->GetGpuMode());
}
#endif  // !OS_FUCHSIA

#if !defined(CAST_AUDIO_ONLY)
TEST_F(GpuDataManagerImplPrivateTest, GpuStartsWithGpuDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableGpu);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::SWIFTSHADER, manager->GetGpuMode());
}
#endif  // !IS_CHROMECAST
#endif  // !OS_ANDROID && !OS_CHROMEOS

// Chromecast audio-only builds should not launch the GPU process.
#if defined(CAST_AUDIO_ONLY)
TEST_F(GpuDataManagerImplPrivateTest, ChromecastStartsWithGpuDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableGpu);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::DISPLAY_COMPOSITOR, manager->GetGpuMode());
}
#endif  // IS_CHROMECAST

#if defined(OS_MAC)
TEST_F(GpuDataManagerImplPrivateTest, FallbackFromMetalToGL) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kMetal);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_METAL, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());
}

TEST_F(GpuDataManagerImplPrivateTest, FallbackFromMetalWithGLDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kMetal);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_METAL, manager->GetGpuMode());

  // Simulate GPU process initialization completing with GL unavailable.
  gpu::GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfoWithOneDisabled(
      gpu::GpuFeatureType::GPU_FEATURE_TYPE_ACCELERATED_GL);
  manager->UpdateGpuFeatureInfo(gpu_feature_info, absl::nullopt);

  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::SWIFTSHADER, manager->GetGpuMode());
}
#endif  // OS_MAC

#if BUILDFLAG(ENABLE_VULKAN)
// TODO(crbug.com/1155622): enable tests when Vulkan is supported on LaCrOS.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(GpuDataManagerImplPrivateTest, GpuStartsWithUseVulkanFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseVulkan, switches::kVulkanImplementationNameNative);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_VULKAN, manager->GetGpuMode());
}

TEST_F(GpuDataManagerImplPrivateTest, GpuStartsWithVulkanFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kVulkan);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_VULKAN, manager->GetGpuMode());
}

// Don't run these tests on Fuchsia, which doesn't support falling back from
// Vulkan.
#if !defined(OS_FUCHSIA)
TEST_F(GpuDataManagerImplPrivateTest, FallbackFromVulkanToGL) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kVulkan);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_VULKAN, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());
}

TEST_F(GpuDataManagerImplPrivateTest, VulkanInitializationFails) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kVulkan);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_VULKAN, manager->GetGpuMode());

  // Simulate GPU process initialization completing with Vulkan unavailable.
  gpu::GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfoWithOneDisabled(
      gpu::GpuFeatureType::GPU_FEATURE_TYPE_VULKAN);
  manager->UpdateGpuFeatureInfo(gpu_feature_info, absl::nullopt);

  // GpuDataManager should update its mode to be GL.
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());

  // The first fallback should go to SwiftShader on platforms where fallback to
  // software is allowed.
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::SWIFTSHADER, manager->GetGpuMode());
#endif  // !OS_ANDROID && !OS_CHROMEOS
}

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(GpuDataManagerImplPrivateTest, FallbackFromVulkanWithGLDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kVulkan);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_VULKAN, manager->GetGpuMode());

  // Simulate GPU process initialization completing with GL unavailable.
  gpu::GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfoWithOneDisabled(
      gpu::GpuFeatureType::GPU_FEATURE_TYPE_ACCELERATED_GL);
  manager->UpdateGpuFeatureInfo(gpu_feature_info, absl::nullopt);

  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::SWIFTSHADER, manager->GetGpuMode());
}
#endif  // !OS_ANDROID && !OS_CHROMEOS
#endif  // !OS_FUCHSIA
#endif  // !IS_CHROMEOS_LACROS
#endif  // BUILDFLAG(ENABLE_VULKAN)

}  // namespace content
