// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/gpu/gpu_data_manager_impl_private.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_domain_guilt.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/memory_stats.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_switches.h"
#include "url/gurl.h"

// TODO(crbug.com/1293538): The IS_CAST_AUDIO_ONLY check should not need to be
// nested inside of an IS_CASTOS check.
#if BUILDFLAG(IS_CASTOS)
#include "chromecast/chromecast_buildflags.h"  // nogncheck
#if BUILDFLAG(IS_CAST_AUDIO_ONLY)
#define CAST_AUDIO_ONLY
#endif  // BUILDFLAG(IS_CAST_AUDIO_ONLY)
#endif  // BUILDFLAG(IS_CASTOS)

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
  return base::Time::FromSecondsSinceUnixEpoch(1000);
}

static GURL GetDomain1ForTesting() {
  return GURL("http://foo.com/");
}

static GURL GetDomain1URL1ForTesting() {
  return GURL("http://foo.com/url1");
}

static GURL GetDomain1URL2ForTesting() {
  return GURL("http://foo.com/url2");
}

static GURL GetDomain2ForTesting() {
  return GURL("http://bar.com/");
}

static GURL GetDomain3ForTesting() {
  return GURL("http://baz.com/");
}

static GURL GetDomain4ForTesting() {
  return GURL("http://yabba.com/");
}

[[maybe_unused]] gpu::GpuFeatureInfo GetGpuFeatureInfoWithOneDisabled(
    gpu::GpuFeatureType disabled_feature) {
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

    ScopedGpuDataManagerImpl(const ScopedGpuDataManagerImpl&) = delete;
    ScopedGpuDataManagerImpl& operator=(const ScopedGpuDataManagerImpl&) =
        delete;

    ~ScopedGpuDataManagerImpl() = default;

    GpuDataManagerImpl* get() { return &impl_; }
    GpuDataManagerImpl* operator->() { return &impl_; }

   private:
    GpuDataManagerImpl impl_;
  };

  // We want to test the code path where GpuDataManagerImplPrivate is created
  // in the GpuDataManagerImpl constructor.
  class ScopedGpuDataManagerImplPrivate {
   public:
    ScopedGpuDataManagerImplPrivate() { EXPECT_TRUE(impl_.private_.get()); }

    ScopedGpuDataManagerImplPrivate(const ScopedGpuDataManagerImplPrivate&) =
        delete;
    ScopedGpuDataManagerImplPrivate& operator=(
        const ScopedGpuDataManagerImplPrivate&) = delete;

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
  };

  base::Time JustBeforeExpiration(const GpuDataManagerImplPrivate* manager);
  base::Time JustAfterExpiration(const GpuDataManagerImplPrivate* manager);
  void TestBlockingDomainFrom3DAPIs(gpu::DomainGuilt guilt_level);
  void TestUnblockingDomainFrom3DAPIs(gpu::DomainGuilt guilt_level);

  base::test::SingleThreadTaskEnvironment task_environment_;
};

class GpuDataManagerImplPrivateTestP
    : public GpuDataManagerImplPrivateTest,
      public testing::WithParamInterface<gpu::DomainGuilt> {};

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
  manager->UpdateGpuInfo(gpu_info, std::nullopt);
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_TRUE(observer.gpu_info_updated());
}

base::Time GpuDataManagerImplPrivateTest::JustBeforeExpiration(
    const GpuDataManagerImplPrivate* manager) {
  return GetTimeForTesting() + manager->GetDomainBlockingExpirationPeriod() -
         base::Milliseconds(3);
}

base::Time GpuDataManagerImplPrivateTest::JustAfterExpiration(
    const GpuDataManagerImplPrivate* manager) {
  return GetTimeForTesting() + manager->GetDomainBlockingExpirationPeriod() +
         base::Milliseconds(3);
}

TEST_P(GpuDataManagerImplPrivateTestP, SingleContextLossDoesNotBlockDomain) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting());

  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
}

TEST_P(GpuDataManagerImplPrivateTestP, TwoContextLossesBlockDomain) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting());
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + base::Seconds(1));

  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
}

TEST_P(GpuDataManagerImplPrivateTestP,
       TwoSimultaneousContextLossesDoNotBlockDomain) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  std::set<GURL> urls;
  urls.insert(GetDomain1URL1ForTesting());
  urls.insert(GetDomain1URL2ForTesting());

  manager->BlockDomainsFrom3DAPIsAtTime(urls, guilt_level, GetTimeForTesting());
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
}

TEST_P(GpuDataManagerImplPrivateTestP, DomainBlockExpires) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting());
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + base::Seconds(1));

  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustAfterExpiration(manager.get())));
}

TEST_P(GpuDataManagerImplPrivateTestP, UnblockDomain) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting());
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + base::Seconds(1));

  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
  manager->UnblockDomainFrom3DAPIs(GetDomain1ForTesting());
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
}

TEST_P(GpuDataManagerImplPrivateTestP, Domain1DoesNotBlockDomain2) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting());
  std::set<GURL> urls;
  urls.insert(GetDomain1ForTesting());
  urls.insert(GetDomain2ForTesting());
  manager->BlockDomainsFrom3DAPIsAtTime(urls, guilt_level,
                                        GetTimeForTesting() + base::Seconds(1));

  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain2ForTesting(),
                                            GetTimeForTesting()));
}

TEST_P(GpuDataManagerImplPrivateTestP, UnblockingDomain1DoesNotUnblockDomain2) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting());
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + base::Seconds(1));
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain2ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + base::Seconds(2));
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain2ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + base::Seconds(3));

  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain2ForTesting(),
                                            GetTimeForTesting()));
  manager->UnblockDomainFrom3DAPIs(GetDomain1ForTesting());
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                            GetTimeForTesting()));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kBlocked,
            manager->Are3DAPIsBlockedAtTime(GetDomain2ForTesting(),
                                            GetTimeForTesting()));
}

TEST_P(GpuDataManagerImplPrivateTestP, SimultaneousContextLossDoesNotBlock) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  std::set<GURL> urls;
  urls.insert(GetDomain1ForTesting());
  urls.insert(GetDomain2ForTesting());
  urls.insert(GetDomain3ForTesting());

  manager->BlockDomainsFrom3DAPIsAtTime(urls, guilt_level, GetTimeForTesting());

  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                      GetTimeForTesting() + base::Seconds(3)));
  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain2ForTesting(),
                                      GetTimeForTesting() + base::Seconds(3)));
  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain3ForTesting(),
                                      GetTimeForTesting() + base::Seconds(3)));
}

TEST_P(GpuDataManagerImplPrivateTestP, MultipleTDRsBlockAll) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  // TDR = Timeout Detection and Recovery.
  base::TimeDelta tdr_interval = base::Seconds(1);

  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting());
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain2ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + tdr_interval);
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain3ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + 2 * tdr_interval);

  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(),
                                      GetTimeForTesting() + 2 * tdr_interval));
  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain2ForTesting(),
                                      GetTimeForTesting() + 2 * tdr_interval));
  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain3ForTesting(),
                                      GetTimeForTesting() + 2 * tdr_interval));
}

TEST_P(GpuDataManagerImplPrivateTestP, MultipleTDRsExpire) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  // TDR = Timeout Detection and Recovery.
  base::TimeDelta tdr_interval = base::Seconds(1);

  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting());
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain2ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + tdr_interval);
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain3ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + 2 * tdr_interval);

  // Note that querying at given times has side effects, so query in
  // order of increasing time.
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain3ForTesting(), JustBeforeExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain1ForTesting(), JustAfterExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain2ForTesting(), JustAfterExpiration(manager.get())));
  EXPECT_EQ(GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
            manager->Are3DAPIsBlockedAtTime(
                GetDomain3ForTesting(), JustAfterExpiration(manager.get())));
}

TEST_P(GpuDataManagerImplPrivateTestP, MultipleTDRsCanBeUnblocked) {
  ScopedGpuDataManagerImplPrivate manager;
  gpu::DomainGuilt guilt_level = GetParam();

  // TDR = Timeout Detection and Recovery.
  base::TimeDelta tdr_interval = base::Seconds(1);

  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain1ForTesting()}}, guilt_level,
                                        GetTimeForTesting());
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain2ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + tdr_interval);
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain3ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + 2 * tdr_interval);
  manager->BlockDomainsFrom3DAPIsAtTime({{GetDomain4ForTesting()}}, guilt_level,
                                        GetTimeForTesting() + 3 * tdr_interval);

  base::Time query_time = JustBeforeExpiration(manager.get());

  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(), query_time));
  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain4ForTesting(), query_time));

  manager->UnblockDomainFrom3DAPIs(GetDomain2ForTesting());

  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(), query_time));
  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kAllDomainsBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain4ForTesting(), query_time));

  manager->UnblockDomainFrom3DAPIs(GetDomain3ForTesting());

  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain1ForTesting(), query_time));
  EXPECT_EQ(
      GpuDataManagerImplPrivate::DomainBlockStatus::kNotBlocked,
      manager->Are3DAPIsBlockedAtTime(GetDomain4ForTesting(), query_time));
}

INSTANTIATE_TEST_SUITE_P(GpuDataManagerImplPrivateTest,
                         GpuDataManagerImplPrivateTestP,
                         ::testing::Values(gpu::DomainGuilt::kKnown,
                                           gpu::DomainGuilt::kUnknown));

TEST_F(GpuDataManagerImplPrivateTest, GpuStartsWithGraphiteFeatureFlag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSkiaGraphite);

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GRAPHITE, manager->GetGpuMode());
}

// On Mac graphite should fallback to Swiftshader immediately. On other
// platforms graphite should fallback to Ganesh/GL.
TEST_F(GpuDataManagerImplPrivateTest, FallbackFromGraphite) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSkiaGraphite);

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GRAPHITE, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());
}

// Android and Chrome OS do not support software compositing, while Fuchsia does
// not support falling back to software from Vulkan.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_IOS)
#if !BUILDFLAG(IS_FUCHSIA)
TEST_F(GpuDataManagerImplPrivateTest, NoDefaultFallbackToSwiftShader) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAllowSwiftShaderFallback);

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::DISPLAY_COMPOSITOR, manager->GetGpuMode());
}

TEST_F(GpuDataManagerImplPrivateTest, ExplicitFallbackToSwiftShader) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableUnsafeSwiftShader);

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::SWIFTSHADER, manager->GetGpuMode());
}

TEST_F(GpuDataManagerImplPrivateTest, FallbackWithSwiftShaderDisabledByFlags) {
  // Make sure that we don't fall back to SwiftShader when it's disabled with
  // --disable-software-rasterizer even if --allow-unsafe-swiftshader is used
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableSoftwareRasterizer);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableUnsafeSwiftShader);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  gpu::GpuMode expected_mode = gpu::GpuMode::DISPLAY_COMPOSITOR;
  EXPECT_EQ(expected_mode, manager->GetGpuMode());
}

TEST_F(GpuDataManagerImplPrivateTest,
       FallbackWithSwiftShaderDisabledByFeatures) {
  // Make sure that we don't fall back to SwiftShader when it's disabled with
  // --disable-software-rasterizer even the AllowSwiftShaderFallback feature is
  // present.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableSoftwareRasterizer);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAllowSwiftShaderFallback);

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  gpu::GpuMode expected_mode = gpu::GpuMode::DISPLAY_COMPOSITOR;
  EXPECT_EQ(expected_mode, manager->GetGpuMode());
}

TEST_F(GpuDataManagerImplPrivateTest,
       FallbackFromGraphiteWithSwiftShaderDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSkiaGraphite);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableSoftwareRasterizer);

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GRAPHITE, manager->GetGpuMode());

  manager->FallBackToNextGpuMode();
  manager->FallBackToNextGpuMode();

  gpu::GpuMode expected_mode = gpu::GpuMode::DISPLAY_COMPOSITOR;
  EXPECT_EQ(expected_mode, manager->GetGpuMode());
}
#endif  // !BUILDFLAG(IS_FUCHSIA)

#if !defined(CAST_AUDIO_ONLY)
TEST_F(GpuDataManagerImplPrivateTest, GpuStartsWithGpuDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kAllowSwiftShaderFallback);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableGpu);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::DISPLAY_COMPOSITOR, manager->GetGpuMode());
}
#endif  // !defined(CAST_AUDIO_ONLY)
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) &&
        // !BUILDFLAG(IS_IOS)

// Chromecast audio-only builds should not launch the GPU process.
#if defined(CAST_AUDIO_ONLY)
TEST_F(GpuDataManagerImplPrivateTest, ChromecastStartsWithGpuDisabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableGpu);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::DISPLAY_COMPOSITOR, manager->GetGpuMode());
}
#endif  // defined(CAST_AUDIO_ONLY)

#if BUILDFLAG(ENABLE_VULKAN)
// TODO(crbug.com/40735511): enable tests when Vulkan is supported on LaCrOS.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(GpuDataManagerImplPrivateTest, GpuStartsWithVulkanFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kVulkan);
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_VULKAN, manager->GetGpuMode());
}

// Don't run these tests on Fuchsia, which doesn't support falling back from
// Vulkan.
#if !BUILDFLAG(IS_FUCHSIA)
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
  feature_list.InitWithFeatures({features::kVulkan},
#if BUILDFLAG(ENABLE_SWIFTSHADER)
                                {features::kAllowSwiftShaderFallback});
#else
                                {});
#endif  // BUILDFLAG(ENABLE_SWIFTSHADER)

  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_VULKAN, manager->GetGpuMode());

  // Simulate GPU process initialization completing with Vulkan unavailable.
  gpu::GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfoWithOneDisabled(
      gpu::GpuFeatureType::GPU_FEATURE_TYPE_VULKAN);
  manager->UpdateGpuFeatureInfo(gpu_feature_info, std::nullopt);

  // GpuDataManager should update its mode to be GL.
  EXPECT_EQ(gpu::GpuMode::HARDWARE_GL, manager->GetGpuMode());

  // The first fallback should go to the display compositor on platforms where
  // fallback to software is allowed.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_IOS)
  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::DISPLAY_COMPOSITOR, manager->GetGpuMode());
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) &&
        // !BUILDFLAG(IS_IOS)
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_IOS)
TEST_F(GpuDataManagerImplPrivateTest, FallbackFromVulkanWithGLDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kVulkan},
                                {features::kAllowSwiftShaderFallback});
  ScopedGpuDataManagerImplPrivate manager;
  EXPECT_EQ(gpu::GpuMode::HARDWARE_VULKAN, manager->GetGpuMode());

  // Simulate GPU process initialization completing with GL unavailable.
  gpu::GpuFeatureInfo gpu_feature_info = GetGpuFeatureInfoWithOneDisabled(
      gpu::GpuFeatureType::GPU_FEATURE_TYPE_ACCELERATED_GL);
  manager->UpdateGpuFeatureInfo(gpu_feature_info, std::nullopt);

  manager->FallBackToNextGpuMode();
  EXPECT_EQ(gpu::GpuMode::DISPLAY_COMPOSITOR, manager->GetGpuMode());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) &&
        // !BUILDFLAG(IS_IOS)
#endif  // !BUILDFLAG(IS_FUCHSIA)
#endif  // !IS_CHROMEOS_LACROS
#endif  // BUILDFLAG(ENABLE_VULKAN)

}  // namespace content
