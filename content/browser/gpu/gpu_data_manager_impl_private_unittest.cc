// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_data_manager_impl_private.h"
#include "content/browser/gpu/gpu_data_manager_testing_autogen.h"
#include "content/browser/gpu/gpu_data_manager_testing_entry_enums_autogen.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/common/memory_stats.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

}  // namespace anonymous

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
  manager->UpdateGpuInfo(gpu_info, base::nullopt);
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

}  // namespace content
