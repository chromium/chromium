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
#include "cc/trees/layer_tree_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/public/dummy_schedulers.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"
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
  void set_emulated_original_screen_infos(
      std::optional<display::ScreenInfos> screen_infos) {
    emulated_original_screen_infos_ = std::move(screen_infos);
  }

  // WidgetBaseClient:
  const display::ScreenInfos& GetOriginalScreenInfos() override {
    if (emulated_original_screen_infos_) {
      return *emulated_original_screen_infos_;
    }
    if (widget_base_ && !widget_base_->screen_infos().screen_infos.empty()) {
      return widget_base_->screen_infos();
    }
    return default_screen_infos_;
  }

  void set_widget_base(WidgetBase* widget_base) { widget_base_ = widget_base; }

 private:
  raw_ptr<WidgetBase> widget_base_ = nullptr;
  display::ScreenInfos default_screen_infos_{display::ScreenInfo()};
  std::optional<display::ScreenInfos> emulated_original_screen_infos_;
};

class WidgetBaseTest : public testing::Test {
 public:
  explicit WidgetBaseTest(
      const std::vector<base::test::FeatureRef>& disabled_features = {})
      : feature_list_({features::kDelayLayerTreeViewDeletionOnLocalSwap,
                       features::kUpdateCompositorMemoryPolicyOnScreenChange},
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
    client_.set_widget_base(widget_base_.get());
    widget_base_->InitializeCompositing(
        *page_scheduler_, client_.GetOriginalScreenInfos(),
        /*settings=*/nullptr, /*frame_widget_input_handler=*/nullptr,
        /*previous_widget=*/nullptr);
  }

  void TearDown() override {
    client_.set_widget_base(nullptr);
    widget_base_.reset();
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
  client_.set_widget_base(nullptr);
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
  client_.set_widget_base(nullptr);
  widget_base_.reset();

  task_environment_.FastForwardBy(
      features::kDelayLayerTreeViewDeletionOnLocalSwapTaskDelayParam.Get());
  EXPECT_EQ(platform_->flush_count(), 0);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(WidgetBaseTest, UpdateScreenInfoUpdatesLayerTreeHostMemoryPolicy) {
  display::ScreenInfo external_screen_info;
  external_screen_info.rect = gfx::Rect(0, 0, 3008, 1692);
  external_screen_info.device_scale_factor = 2.0f;
  const display::ScreenInfos external_screen_infos(external_screen_info);
  widget_base_->UpdateScreenInfo(external_screen_infos);

  const cc::ManagedMemoryPolicy expected_external_policy =
      GetGpuMemoryPolicy(widget_base_->LayerTreeHost()->memory_policy(),
                         external_screen_info.rect.size(),
                         external_screen_info.device_scale_factor);
  EXPECT_EQ(expected_external_policy,
            widget_base_->LayerTreeHost()->memory_policy());

  // Emulating a smaller mobile screen while GetOriginalScreenInfos() remains on
  // the external display should keep the physical display's memory policy.
  client_.set_emulated_original_screen_infos(external_screen_infos);
  display::ScreenInfo emulated_mobile_screen_info;
  emulated_mobile_screen_info.rect = gfx::Rect(0, 0, 375, 667);
  emulated_mobile_screen_info.device_scale_factor = 2.0f;
  widget_base_->UpdateScreenInfo(
      display::ScreenInfos(emulated_mobile_screen_info));
  EXPECT_EQ(expected_external_policy,
            widget_base_->LayerTreeHost()->memory_policy());
  client_.set_emulated_original_screen_infos(std::nullopt);

  display::ScreenInfo internal_screen_info;
  internal_screen_info.rect = gfx::Rect(0, 0, 1440, 900);
  internal_screen_info.device_scale_factor = 2.0f;
  const display::ScreenInfos internal_screen_infos(internal_screen_info);
  widget_base_->UpdateScreenInfo(internal_screen_infos);

  const cc::ManagedMemoryPolicy expected_internal_policy =
      GetGpuMemoryPolicy(widget_base_->LayerTreeHost()->memory_policy(),
                         internal_screen_info.rect.size(),
                         internal_screen_info.device_scale_factor);
  EXPECT_EQ(expected_internal_policy,
            widget_base_->LayerTreeHost()->memory_policy());
  EXPECT_LT(expected_internal_policy.bytes_limit_when_visible,
            expected_external_policy.bytes_limit_when_visible);

  widget_base_->Shutdown(/*delay_release=*/false);
}

TEST_F(WidgetBaseTest,
       InitializeCompositingWithPreviousWidgetUpdatesMemoryPolicy) {
  display::ScreenInfo internal_screen_info;
  internal_screen_info.rect = gfx::Rect(0, 0, 1440, 900);
  internal_screen_info.device_scale_factor = 2.0f;
  widget_base_->UpdateScreenInfo(display::ScreenInfos(internal_screen_info));

  const cc::ManagedMemoryPolicy expected_internal_policy =
      GetGpuMemoryPolicy(widget_base_->LayerTreeHost()->memory_policy(),
                         internal_screen_info.rect.size(),
                         internal_screen_info.device_scale_factor);
  EXPECT_EQ(expected_internal_policy,
            widget_base_->LayerTreeHost()->memory_policy());

  display::ScreenInfo external_screen_info;
  external_screen_info.rect = gfx::Rect(0, 0, 3008, 1692);
  external_screen_info.device_scale_factor = 2.0f;
  const display::ScreenInfos external_screen_infos(external_screen_info);

  mojo::AssociatedRemote<mojom::blink::WidgetHost> next_widget_host;
  std::ignore = next_widget_host.BindNewEndpointAndPassDedicatedReceiver();
  mojo::AssociatedRemote<mojom::blink::Widget> next_widget_remote;
  auto next_widget = std::make_unique<WidgetBase>(
      &client_, next_widget_host.Unbind(),
      next_widget_remote.BindNewEndpointAndPassDedicatedReceiver(),
      scheduler::GetSingleThreadTaskRunnerForTesting(), /*hidden=*/true,
      /*never_composited=*/false, /*is_embedded=*/false,
      /*is_for_scalable_page=*/false);
  client_.set_widget_base(next_widget.get());
  next_widget->InitializeCompositing(*page_scheduler_, external_screen_infos,
                                     /*settings=*/nullptr,
                                     /*frame_widget_input_handler=*/nullptr,
                                     /*previous_widget=*/widget_base_.get());

  const cc::ManagedMemoryPolicy expected_external_policy =
      GetGpuMemoryPolicy(next_widget->LayerTreeHost()->memory_policy(),
                         external_screen_info.rect.size(),
                         external_screen_info.device_scale_factor);
  EXPECT_EQ(expected_external_policy,
            next_widget->LayerTreeHost()->memory_policy());

  client_.set_widget_base(nullptr);
  widget_base_->Shutdown(/*delay_release=*/false);
  next_widget->Shutdown(/*delay_release=*/false);
}

class WidgetBaseMemoryPolicyKillSwitchTest : public WidgetBaseTest {
 public:
  WidgetBaseMemoryPolicyKillSwitchTest()
      : WidgetBaseTest(
            {features::kUpdateCompositorMemoryPolicyOnScreenChange}) {}
};

TEST_F(WidgetBaseMemoryPolicyKillSwitchTest,
       UpdateScreenInfoDoesNotUpdateLayerTreeHostMemoryPolicyWhenDisabled) {
  const cc::ManagedMemoryPolicy initial_policy =
      widget_base_->LayerTreeHost()->memory_policy();

  display::ScreenInfo external_screen_info;
  external_screen_info.rect = gfx::Rect(0, 0, 3008, 1692);
  external_screen_info.device_scale_factor = 2.0f;
  widget_base_->UpdateScreenInfo(display::ScreenInfos(external_screen_info));

  EXPECT_EQ(initial_policy, widget_base_->LayerTreeHost()->memory_policy());

  widget_base_->Shutdown(/*delay_release=*/false);
}
#endif

}  // namespace
}  // namespace blink
