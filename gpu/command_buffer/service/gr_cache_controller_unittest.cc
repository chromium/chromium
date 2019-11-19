// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gr_cache_controller.h"

#include "base/bind_helpers.h"
#include "base/test/test_mock_time_task_runner.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gpu {
namespace raster {

class GrCacheControllerTest : public testing::Test {
 public:
  void SetUp() override {
    gl::GLSurfaceTestSupport::InitializeOneOff();
    gpu::GpuDriverBugWorkarounds workarounds;

    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    scoped_refptr<gl::GLSurface> surface =
        gl::init::CreateOffscreenGLSurface(gfx::Size());
    scoped_refptr<gl::GLContext> context = gl::init::CreateGLContext(
        share_group.get(), surface.get(), gl::GLContextAttribs());
    ASSERT_TRUE(context->MakeCurrent(surface.get()));

    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), std::move(surface), std::move(context),
        false /* use_virtualized_gl_contexts */, base::DoNothing());
    context_state_->InitializeGrContext(workarounds, nullptr);
    auto feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    context_state_->InitializeGL(GpuPreferences(), std::move(feature_info));

    controller_ =
        std::make_unique<GrCacheController>(context_state_.get(), task_runner_);
  }

  void TearDown() override {
    controller_ = nullptr;
    context_state_ = nullptr;
    task_runner_ = nullptr;
    gl::init::ShutdownGL(false);
  }

  GrContext* gr_context() { return context_state_->gr_context(); }

 protected:
  scoped_refptr<SharedContextState> context_state_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<GrCacheController> controller_;
};

TEST_F(GrCacheControllerTest, PurgeGrCache) {
  EXPECT_EQ(gr_context()->getResourceCachePurgeableBytes(), 0u);
  {
    // Use the GrContext to upload an image.
    SkBitmap bm;
    SkImageInfo info = SkImageInfo::MakeN32Premul(10, 10);
    ASSERT_TRUE(bm.tryAllocPixels(info));
    sk_sp<SkImage> uploaded =
        SkImage::MakeFromBitmap(bm)->makeTextureImage(gr_context());
    ASSERT_TRUE(uploaded);
  }
  EXPECT_GT(gr_context()->getResourceCachePurgeableBytes(), 0u);

  // We should have a pending task to clear the cache.
  controller_->ScheduleGrContextCleanup();
  EXPECT_TRUE(task_runner_->HasPendingTask());

  // Fast forward by a second, the task runs and the cache is cleared.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(gr_context()->getResourceCachePurgeableBytes(), 0u);
}

TEST_F(GrCacheControllerTest, ResetPurgeGrCacheOnReuse) {
  EXPECT_EQ(gr_context()->getResourceCachePurgeableBytes(), 0u);
  {
    // Use the GrContext to upload an image.
    SkBitmap bm;
    SkImageInfo info = SkImageInfo::MakeN32Premul(10, 10);
    ASSERT_TRUE(bm.tryAllocPixels(info));
    sk_sp<SkImage> uploaded =
        SkImage::MakeFromBitmap(bm)->makeTextureImage(gr_context());
    ASSERT_TRUE(uploaded);
  }
  EXPECT_GT(gr_context()->getResourceCachePurgeableBytes(), 0u);

  // We should have a pending task to clear the cache.
  controller_->ScheduleGrContextCleanup();
  EXPECT_TRUE(task_runner_->HasPendingTask());

  // Reuse the context. This should push clearing of the cache further by a
  // second.
  controller_->ScheduleGrContextCleanup();

  // Fast forward by a second, the task runs but since the context was used
  // since the task was posted, the cache is not cleared.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_GT(gr_context()->getResourceCachePurgeableBytes(), 0u);

  // Fast forward by another second. Since there is no activity, the cache is
  // cleared.
  task_runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(gr_context()->getResourceCachePurgeableBytes(), 0u);
}

}  // namespace raster
}  // namespace gpu
