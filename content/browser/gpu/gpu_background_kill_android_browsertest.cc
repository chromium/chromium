// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/run_loop.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/compositor_dependencies_android.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/test/gpu_browsertest_helpers.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using base::RepeatingClosure;
using base::RunLoop;

class BackgroundKillGpuManagerObserver : public GpuDataManagerObserver {
 public:
  explicit BackgroundKillGpuManagerObserver(RepeatingClosure callback)
      : callback_(std::move(callback)) {
    GpuDataManagerImpl::GetInstance()->AddObserver(this);
  }
  ~BackgroundKillGpuManagerObserver() override {
    GpuDataManagerImpl::GetInstance()->RemoveObserver(this);
  }
  void OnGpuProcessCrashed() override { callback_.Run(); }

 private:
  RepeatingClosure callback_;
};

class GpuKillBackgroundTest : public ContentBrowserTest {
 public:
  GpuKillBackgroundTest() = default;

  bool IsChannelEstablished() {
    return gpu_channel_host_ && !gpu_channel_host_->IsLost();
  }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
};

// TODO(crbug.com/40926381): Flaky.
IN_PROC_BROWSER_TEST_F(GpuKillBackgroundTest, DISABLED_Simple) {
  ASSERT_FALSE(IsChannelEstablished());

  gpu_channel_host_ = GpuBrowsertestEstablishGpuChannelSyncRunLoop();

  ASSERT_TRUE(IsChannelEstablished());

  auto& cda = CompositorDependenciesAndroid::Get();

  RunLoop run_loop;
  BackgroundKillGpuManagerObserver observer(run_loop.QuitClosure());

  cda.DoLowEndBackgroundCleanupForTesting();

  run_loop.Run();

  ASSERT_FALSE(IsChannelEstablished());
}

}  // namespace content
