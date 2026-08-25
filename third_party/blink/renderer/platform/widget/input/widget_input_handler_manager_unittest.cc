// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/widget_input_handler_manager.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_restrictions.h"
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
namespace blink::test {

class WidgetInputHandlerManagerTest : public testing::Test {
 public:
  WidgetInputHandlerManagerTest() = default;
  ~WidgetInputHandlerManagerTest() override = default;

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

 protected:
  base::test::TaskEnvironment task_environment_;

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
};

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

  // WidgetBase isn't setup with full mocks. Were we to call `InitInputHandler`
  // we'd attempt to access `LayerHostTree` which we can't mock in WidgetBase.
  // So we do not call it, but we set needs_input_handler=true to satisfy
  // DCHECKs.
  widget_input_handler_manager_ = WidgetInputHandlerManager::Create(
      widget_base_->GetWeakPtr(), frame_widget_input_handler_, never_composited,
      /*compositor_thread_scheduler=*/nullptr, widget_scheduler_,
      /*uses_input_handler=*/false,
      /*allow_scroll_resampling=*/false,
      /*io_thread_id=*/base::kInvalidThreadId,
      /*main_thread_id=*/base::PlatformThread::CurrentId());

  auto unique_input_handler_proxy = std::make_unique<MockInputHandlerProxy>(
      mock_input_handler_, &mock_client_);
  input_handler_proxy_ = unique_input_handler_proxy.get();
  widget_input_handler_manager_->SetInputHandlerProxyForTesting(
      std::move(unique_input_handler_proxy));
}

TEST_F(WidgetInputHandlerManagerTest, DISABLED_VizHostRace) {
  const int kOpsPerThread = 1000;

  scoped_refptr<WidgetInputHandlerManager> manager =
      WidgetInputHandlerManager::Create(
          widget_base_->GetWeakPtr(), frame_widget_input_handler_,
          /*never_composited=*/false,
          /*compositor_thread_scheduler=*/nullptr, widget_scheduler_,
          /*uses_input_handler=*/false,
          /*allow_scroll_resampling=*/false,
          /*io_thread_id=*/base::kInvalidThreadId,
          /*main_thread_id=*/base::PlatformThread::CurrentId());

  base::WaitableEvent start_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent worker_ready(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::RunLoop run_loop;

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(
          [](scoped_refptr<WidgetInputHandlerManager> manager,
             base::WaitableEvent* ready, base::WaitableEvent* start, int ops) {
            ready->Signal();
            start->Wait();
            for (int i = 0; i < ops; ++i) {
              manager->GetVizWidgetInputHandlerHost();
            }
          },
          manager, &worker_ready, &start_event, kOpsPerThread),
      run_loop.QuitClosure());

  worker_ready.Wait();
  start_event.Signal();

  std::vector<mojo::PendingReceiver<mojom::blink::WidgetInputHandlerHost>>
      receivers;
  for (int i = 0; i < kOpsPerThread; ++i) {
    mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> viz_host_remote;
    auto receiver = viz_host_remote.InitWithNewPipeAndPassReceiver();
    receivers.push_back(std::move(receiver));
    manager->SetVizHost(std::move(viz_host_remote));
  }

  run_loop.Run();
}

TEST_F(WidgetInputHandlerManagerTest, NoLeakWithoutDisconnect) {
  scoped_refptr<WidgetInputHandlerManager> manager =
      WidgetInputHandlerManager::Create(
          widget_base_->GetWeakPtr(), frame_widget_input_handler_,
          /*never_composited=*/false,
          /*compositor_thread_scheduler=*/nullptr, widget_scheduler_,
          /*uses_input_handler=*/false,
          /*allow_scroll_resampling=*/false,
          /*io_thread_id=*/base::kInvalidThreadId,
          /*main_thread_id=*/base::PlatformThread::CurrentId());

  mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> viz_host_remote;
  auto receiver = viz_host_remote.InitWithNewPipeAndPassReceiver();

  manager->SetVizHost(std::move(viz_host_remote));

  base::RunLoop run_loop;
  manager->set_destruction_callback_for_testing(run_loop.QuitClosure());

  // Clear client (simulating shutdown) which should break the ref cycle.
  manager->ClearClient();

  // Drop the main reference.
  manager = nullptr;

  // Wait until the manager is destroyed.
  run_loop.Run();
}

TEST_F(WidgetInputHandlerManagerTest, ClearClientBreaksCycleEvenIfCopiesExist) {
  scoped_refptr<WidgetInputHandlerManager> manager =
      WidgetInputHandlerManager::Create(
          widget_base_->GetWeakPtr(), frame_widget_input_handler_,
          /*never_composited=*/false,
          /*compositor_thread_scheduler=*/nullptr, widget_scheduler_,
          /*uses_input_handler=*/false,
          /*allow_scroll_resampling=*/false,
          /*io_thread_id=*/base::kInvalidThreadId,
          /*main_thread_id=*/base::PlatformThread::CurrentId());

  mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> viz_host_remote;
  auto receiver = viz_host_remote.InitWithNewPipeAndPassReceiver();

  manager->SetVizHost(std::move(viz_host_remote));

  // Get a copy of the SharedRemote and keep it alive in the test.
  auto viz_host_copy = manager->GetVizWidgetInputHandlerHost();

  base::RunLoop run_loop;
  manager->set_destruction_callback_for_testing(run_loop.QuitClosure());

  // Call ClearClient(). If the fix is active, this should call
  // viz_host_.Disconnect() and break the cycle even though viz_host_copy is
  // still alive.
  manager->ClearClient();

  // Drop the main reference.
  manager = nullptr;

  // The manager should be destroyed.
  run_loop.Run();
}

TEST_F(WidgetInputHandlerManagerTest, ClearClientBreaksCycleEvenIfCopiesExistForHost) {
  scoped_refptr<WidgetInputHandlerManager> manager =
      WidgetInputHandlerManager::Create(
          widget_base_->GetWeakPtr(), frame_widget_input_handler_,
          /*never_composited=*/false,
          /*compositor_thread_scheduler=*/nullptr, widget_scheduler_,
          /*uses_input_handler=*/false,
          /*allow_scroll_resampling=*/false,
          /*io_thread_id=*/base::kInvalidThreadId,
          /*main_thread_id=*/base::PlatformThread::CurrentId());

  mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> host_remote;
  auto receiver = host_remote.InitWithNewPipeAndPassReceiver();

  manager->SetHost(std::move(host_remote));

  // Get a copy of the SharedRemote and keep it alive in the test.
  auto host_copy = manager->GetWidgetInputHandlerHost();

  base::RunLoop run_loop;
  manager->set_destruction_callback_for_testing(run_loop.QuitClosure());

  // Call ClearClient(). This should call host_.Disconnect() and break the cycle
  // even though host_copy is still alive.
  manager->ClearClient();

  // Drop the main reference.
  manager = nullptr;

  // The manager should be destroyed.
  run_loop.Run();
}

TEST_F(WidgetInputHandlerManagerTest, MultiThreadedHostAccess) {
  const int kOpsPerThread = 1000;

  scoped_refptr<WidgetInputHandlerManager> manager =
      WidgetInputHandlerManager::Create(
          widget_base_->GetWeakPtr(), frame_widget_input_handler_,
          /*never_composited=*/false,
          /*compositor_thread_scheduler=*/nullptr, widget_scheduler_,
          /*uses_input_handler=*/false,
          /*allow_scroll_resampling=*/false,
          /*io_thread_id=*/base::kInvalidThreadId,
          /*main_thread_id=*/base::PlatformThread::CurrentId());

  mojo::PendingRemote<mojom::blink::WidgetInputHandlerHost> host_remote;
  auto receiver = host_remote.InitWithNewPipeAndPassReceiver();
  manager->SetHost(std::move(host_remote));

  base::WaitableEvent start_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent worker_ready(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::RunLoop run_loop;

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(
          [](scoped_refptr<WidgetInputHandlerManager> manager,
             base::WaitableEvent* ready, base::WaitableEvent* start, int ops) {
            ready->Signal();
            start->Wait();
            for (int i = 0; i < ops; ++i) {
              manager->GetWidgetInputHandlerHost();
            }
          },
          manager, &worker_ready, &start_event, kOpsPerThread),
      run_loop.QuitClosure());

  worker_ready.Wait();
  start_event.Signal();

  for (int i = 0; i < kOpsPerThread; ++i) {
    manager->GetWidgetInputHandlerHost();
  }

  run_loop.Run();
}

}  // namespace blink::test
