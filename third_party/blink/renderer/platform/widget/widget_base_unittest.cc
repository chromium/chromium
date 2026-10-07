// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/widget_base.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/widget/compositing/test/stub_widget_base_client.h"
#include "ui/display/screen_info.h"
#include "ui/display/screen_infos.h"

namespace blink {
namespace {

// Counts FlushGpuChannelIfEstablished() calls.
class FlushCountingTestingPlatformSupport : public TestingPlatformSupport {
 public:
  // Platform:
  void FlushGpuChannelIfEstablished() override { ++flush_count_; }

  int flush_count() const { return flush_count_; }

 private:
  int flush_count_ = 0;
};

class TestWidgetBaseClient : public StubWidgetBaseClient {
 public:
  // WidgetBaseClient:
  const display::ScreenInfos& GetOriginalScreenInfos() override {
    return screen_infos_;
  }

 private:
  display::ScreenInfos screen_infos_{display::ScreenInfo()};
};

class WidgetBaseTest : public testing::Test {
 public:
  explicit WidgetBaseTest(
      const std::vector<base::test::FeatureRef>& disabled_features = {})
      : feature_list_({features::kDelayLayerTreeViewDeletionOnLocalSwap},
                      disabled_features) {}

  void SetUp() override {
    mojo::AssociatedRemote<mojom::blink::WidgetHost> widget_host;
    std::ignore = widget_host.BindNewEndpointAndPassDedicatedReceiver();
    mojo::AssociatedRemote<mojom::blink::Widget> widget;
    // The widget is hidden, so it never requests a LayerTreeFrameSink.
    widget_base_ = std::make_unique<WidgetBase>(
        &client_, widget_host.Unbind(),
        widget.BindNewEndpointAndPassDedicatedReceiver(),
        scheduler::GetSingleThreadTaskRunnerForTesting(), /*hidden=*/true,
        /*never_composited=*/false, /*is_embedded=*/false,
        /*is_for_scalable_page=*/false);
    widget_base_->InitializeCompositing(
        *page_scheduler_, client_.GetOriginalScreenInfos(),
        /*settings=*/nullptr, /*frame_widget_input_handler=*/nullptr,
        /*previous_widget=*/nullptr);
  }

 protected:
  // Declared before `task_environment_`, so that the features are set before
  // its thread pool starts, and reset only after the thread pool has shut down.
  // Thread pool threads also check features outside of tasks (e.g. on Android,
  // when they start), so changing features while they run is racy.
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingPlatformSupport<FlushCountingTestingPlatformSupport> platform_;
  TestWidgetBaseClient client_;
  std::unique_ptr<PageScheduler> page_scheduler_ =
      scheduler::CreateDummyPageScheduler();
  std::unique_ptr<WidgetBase> widget_base_;
};

// Releasing a LayerTreeView destroys its SharedImages. The destruction requests
// are deferred GPU channel messages, so the channel needs to be flushed right
// after the (delayed) release.
TEST_F(WidgetBaseTest, ReleasingLayerTreeViewFlushesGpuChannel) {
  const base::TimeDelta delay =
      features::kDelayLayerTreeViewDeletionOnLocalSwapTaskDelayParam.Get();
  widget_base_->Shutdown(/*delay_release=*/true);
  widget_base_.reset();

  task_environment_.FastForwardBy(delay - base::Milliseconds(1));
  EXPECT_EQ(platform_->flush_count(), 0);

  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(platform_->flush_count(), 1);
}

// WidgetBaseTest with the flush disabled by its kill switch.
class WidgetBaseKillSwitchTest : public WidgetBaseTest {
 public:
  WidgetBaseKillSwitchTest()
      : WidgetBaseTest({features::kFlushGpuChannelOnLayerTreeViewRelease}) {}
};

TEST_F(WidgetBaseKillSwitchTest, ReleasingLayerTreeViewDoesNotFlushGpuChannel) {
  widget_base_->Shutdown(/*delay_release=*/true);
  widget_base_.reset();

  task_environment_.FastForwardBy(
      features::kDelayLayerTreeViewDeletionOnLocalSwapTaskDelayParam.Get());
  EXPECT_EQ(platform_->flush_count(), 0);
}

}  // namespace
}  // namespace blink
