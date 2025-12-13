// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/mock_input_handler.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/trees/layer_tree_settings.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_widget_scheduler.h"
#include "third_party/blink/renderer/platform/widget/compositing/test/stub_widget_base_client.h"
#include "third_party/blink/renderer/platform/widget/input/frame_widget_input_handler_impl.h"
#include "third_party/blink/renderer/platform/widget/input/input_handler_proxy.h"
#include "third_party/blink/renderer/platform/widget/input/mock_input_handler_proxy.h"
#include "third_party/blink/renderer/platform/widget/input/mock_input_handler_proxy_client.h"
#include "third_party/blink/renderer/platform/widget/widget_base.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink ::test {

class WidgetInputHandlerManagerTest : public testing::Test,
                                      public testing::WithParamInterface<bool> {
 public:
  WidgetInputHandlerManagerTest();
  ~WidgetInputHandlerManagerTest() override = default;

  bool IgnoreInputWhileHidden() const { return GetParam(); }

  // Generates and empty touch start, and invokes `DispatchEvent`. Will validate
  // that `HandleInputEventWithLatencyInfo` is called `expected_times_called`.
  void DispatchTouchEvent(
      int expected_times_called,
      mojom::blink::WidgetInputHandler::DispatchEventCallback callback);

  // Validates that `HandleInputEventWithLatencyInfo` is not called, and that
  // the `DispatchEventCallback` is notified that the event was not consumed.
  void ExpectNotConsumedDispatchEvent();

  scoped_refptr<WidgetInputHandlerManager> widget_input_handler_manager() {
    return widget_input_handler_manager_;
  }

  // testing::Test:
  void SetUp() override;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  scoped_refptr<WidgetInputHandlerManager> widget_input_handler_manager_;
  StubWidgetBaseClient client_;
  scoped_refptr<scheduler::FakeWidgetScheduler> widget_scheduler_ =
      base::MakeRefCounted<scheduler::FakeWidgetScheduler>();
  std::unique_ptr<WidgetBase> widget_base_;
  base::WeakPtr<mojom::blink::FrameWidgetInputHandler>
      frame_widget_input_handler_;

  testing::StrictMock<cc::MockInputHandler> mock_input_handler_;
  raw_ptr<MockInputHandlerProxy> input_handler_proxy_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

WidgetInputHandlerManagerTest::WidgetInputHandlerManagerTest() {
  if (IgnoreInputWhileHidden()) {
    scoped_feature_list_.InitAndEnableFeature(
        features::kIgnoreInputWhileHidden);
  } else {
    scoped_feature_list_.InitAndDisableFeature(
        features::kIgnoreInputWhileHidden);
  }
}

void WidgetInputHandlerManagerTest::DispatchTouchEvent(
    int expected_times_called,
    mojom::blink::WidgetInputHandler::DispatchEventCallback callback) {
  EXPECT_CALL(*input_handler_proxy_, HandleInputEventWithLatencyInfo(
                                         testing::_, testing::_, testing::_))
      .Times(expected_times_called);
  widget_input_handler_manager_->DispatchEvent(
      std::make_unique<WebCoalescedInputEvent>(
          WebTouchEvent(WebInputEvent::Type::kTouchStart,
                        WebInputEvent::kNoModifiers,
                        WebInputEvent::GetStaticTimeStampForTests()),
          ui::LatencyInfo()),
      std::move(callback));
  testing::Mock::VerifyAndClearExpectations(input_handler_proxy_);
}

void WidgetInputHandlerManagerTest::ExpectNotConsumedDispatchEvent() {
  DispatchTouchEvent(
      /*expected_times_called=*/0,
      BindOnce([](mojom::blink::InputEventResultSource, const ui::LatencyInfo&,
                  mojom::blink::InputEventResultState result_state,
                  mojom::blink::DidOverscrollParamsPtr,
                  mojom::blink::TouchActionOptionalPtr) {
        EXPECT_EQ(result_state,
                  mojom::blink::InputEventResultState::kNotConsumed);
      }));
}

void WidgetInputHandlerManagerTest::SetUp() {
  mojo::AssociatedRemote<mojom::blink::Widget> widget_remote;
  mojo::PendingAssociatedReceiver<mojom::blink::Widget> widget_receiver =
      widget_remote.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<mojom::blink::WidgetHost> widget_host_remote;
  std::ignore = widget_host_remote.BindNewEndpointAndPassDedicatedReceiver();

  const bool never_composited = false;

  widget_base_ = std::make_unique<WidgetBase>(
      /*widget_base_client=*/&client_, widget_host_remote.Unbind(),
      std::move(widget_receiver),
      scheduler::GetSingleThreadTaskRunnerForTesting(),
      /*is_hidden=*/false, never_composited,
      /*is_for_child_local_root=*/false,
      /*is_for_scalable_page=*/true);

  mojo::AssociatedRemote<mojom::blink::FrameWidgetInputHandler>
      frame_widget_input_handler_remote;
  mojo::PendingAssociatedReceiver<mojom::blink::FrameWidgetInputHandler>
      frame_widget_input_handler_receiver =
          frame_widget_input_handler_remote
              .BindNewEndpointAndPassDedicatedReceiver();

  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<FrameWidgetInputHandlerImpl>(
          widget_base_->GetWeakPtr(), frame_widget_input_handler_,
          /*input_event_queue=*/nullptr),
      std::move(frame_widget_input_handler_receiver));

  // WidgetBase isn't setup with full mocks. Were we to call this with
  // `uses_input_hanlder_=true` we'd attempt to access `LayerHostTree` which
  // we can't mock in WidgetBase. So we set to false, and use test override
  // to enable the desired dispach mode.
  widget_input_handler_manager_ = WidgetInputHandlerManager::Create(
      widget_base_->GetWeakPtr(), frame_widget_input_handler_, never_composited,
      /*compositor_thread_scheduler=*/nullptr, widget_scheduler_,
      /*needs_input_handler=*/false,
      /*allow_scroll_resampling=*/false,
      /*io_thread_id=*/base::kInvalidThreadId,
      /*main_thread_id=*/base::PlatformThread::CurrentId());

  auto unique_input_handler_proxy = std::make_unique<MockInputHandlerProxy>(
      mock_input_handler_, &mock_client_);
  input_handler_proxy_ = unique_input_handler_proxy.get();
  widget_input_handler_manager_->SetInputHandlerProxyForTesting(
      std::move(unique_input_handler_proxy));
}

// Tests that while we are hidden, that input is neither dispatched nor
// consumed. Becoming visible should remove this suppression.
TEST_P(WidgetInputHandlerManagerTest, InputWhileHidden) {
  auto manager = widget_input_handler_manager();
  EXPECT_EQ(
      manager->suppressing_input_events_state(),
      static_cast<uint16_t>(WidgetInputHandlerManager::
                                SuppressingInputEventsBits::kHasNotPainted));
  manager->OnFirstContentfulPaint(base::TimeTicks::Now());
  EXPECT_EQ(manager->suppressing_input_events_state(), 0u);

  manager->SetHidden(true);
  EXPECT_EQ(
      manager->suppressing_input_events_state(),
      static_cast<uint16_t>(
          WidgetInputHandlerManager::SuppressingInputEventsBits::kHidden));

  if (GetParam()) {
    ExpectNotConsumedDispatchEvent();
  }

  manager->SetHidden(false);
  EXPECT_EQ(manager->suppressing_input_events_state(), 0u);
  DispatchTouchEvent(/*expected_times_called=*/1,
                     mojom::blink::WidgetInputHandler::DispatchEventCallback());
}

// Tests that while we are hidden, and attached DevTools sessions witll override
// the input suppression. Events should be dispatched. Upon the session
// detaching we should resume neither dispatching nor consuming events.
TEST_P(WidgetInputHandlerManagerTest, DevToolsSessionOverridesSuppression) {
  auto manager = widget_input_handler_manager();
  EXPECT_EQ(
      manager->suppressing_input_events_state(),
      static_cast<uint16_t>(WidgetInputHandlerManager::
                                SuppressingInputEventsBits::kHasNotPainted));
  manager->OnFirstContentfulPaint(base::TimeTicks::Now());
  EXPECT_EQ(manager->suppressing_input_events_state(), 0u);

  manager->SetHidden(true);
  EXPECT_EQ(
      manager->suppressing_input_events_state(),
      static_cast<uint16_t>(
          WidgetInputHandlerManager::SuppressingInputEventsBits::kHidden));

  manager->OnDevToolsSessionConnectionChanged(/*attached=*/true);
  DispatchTouchEvent(/*expected_times_called=*/1,
                     mojom::blink::WidgetInputHandler::DispatchEventCallback());

  manager->OnDevToolsSessionConnectionChanged(/*attached=*/false);
  if (GetParam()) {
    ExpectNotConsumedDispatchEvent();
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         WidgetInputHandlerManagerTest,
                         testing::Bool(),
                         [](auto& info) {
                           return info.param ? "IgnoreInputWhileHidden"
                                             : "ProcessInputWhileHidden";
                         });
}  // namespace blink::test
