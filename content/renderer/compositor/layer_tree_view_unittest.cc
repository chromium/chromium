// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/compositor/layer_tree_view.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/test/fake_layer_tree_frame_sink.h"
#include "cc/test/test_task_graph_runner.h"
#include "cc/test/test_ukm_recorder_factory.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/test/test_context_provider.h"
#include "content/test/stub_layer_tree_view_delegate.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/scheduler/test/web_fake_thread_scheduler.h"

using testing::AllOf;
using testing::Field;

namespace content {
namespace {

enum FailureMode {
  NO_FAILURE,
  BIND_CONTEXT_FAILURE,
  GPU_CHANNEL_FAILURE,
};

class FakeLayerTreeViewDelegate : public StubLayerTreeViewDelegate {
 public:
  FakeLayerTreeViewDelegate() = default;

  void RequestNewLayerTreeFrameSink(
      LayerTreeFrameSinkCallback callback) override {
    // Subtract one cuz the current request has already been counted but should
    // not be included for this.
    if (num_requests_since_last_success_ - 1 < num_requests_before_success_) {
      std::move(callback).Run(nullptr);
      return;
    }

    auto context_provider = viz::TestContextProvider::Create();
    if (num_failures_since_last_success_ < num_failures_before_success_) {
      context_provider->UnboundTestContextGL()->LoseContextCHROMIUM(
          GL_GUILTY_CONTEXT_RESET_ARB, GL_INNOCENT_CONTEXT_RESET_ARB);
    }
    std::move(callback).Run(
        cc::FakeLayerTreeFrameSink::Create3d(std::move(context_provider)));
  }

  void Reset() {
    num_requests_ = 0;
    num_requests_before_success_ = 0;
    num_requests_since_last_success_ = 0;
    num_failures_ = 0;
    num_failures_before_success_ = 0;
    num_failures_since_last_success_ = 0;
    num_successes_ = 0;
  }

  void add_success() {
    ++num_successes_;
    num_requests_since_last_success_ = 0;
    num_failures_since_last_success_ = 0;
  }
  int num_successes() const { return num_successes_; }

  void add_request() {
    ++num_requests_since_last_success_;
    ++num_requests_;
  }
  int num_requests() const { return num_requests_; }

  void add_failure() {
    ++num_failures_since_last_success_;
    ++num_failures_;
  }
  int num_failures() const { return num_failures_; }

  void set_num_requests_before_success(int n) {
    num_requests_before_success_ = n;
  }
  void set_num_failures_before_success(int n) {
    num_failures_before_success_ = n;
  }
  int num_failures_before_success() const {
    return num_failures_before_success_;
  }

 private:
  int num_requests_ = 0;
  int num_requests_before_success_ = 0;
  int num_requests_since_last_success_ = 0;
  int num_failures_ = 0;
  int num_failures_before_success_ = 0;
  int num_failures_since_last_success_ = 0;
  int num_successes_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeLayerTreeViewDelegate);
};

// Verify that failing to create an output surface will cause the compositor
// to attempt to repeatedly create another output surface.
// The use null output surface parameter allows testing whether failures
// from RenderWidget (couldn't create an output surface) vs failures from
// the compositor (couldn't bind the output surface) are handled identically.
class LayerTreeViewWithFrameSinkTracking : public LayerTreeView {
 public:
  LayerTreeViewWithFrameSinkTracking(
      FakeLayerTreeViewDelegate* delegate,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_thread,
      cc::TaskGraphRunner* task_graph_runner,
      blink::scheduler::WebThreadScheduler* scheduler)
      : LayerTreeView(delegate,
                      std::move(main_thread),
                      std::move(compositor_thread),
                      task_graph_runner,
                      scheduler),
        delegate_(delegate) {}

  // Force a new output surface to be created.
  void SynchronousComposite() {
    layer_tree_host()->SetVisible(false);
    layer_tree_host()->ReleaseLayerTreeFrameSink();
    layer_tree_host()->SetVisible(true);

    base::TimeTicks some_time;
    layer_tree_host()->Composite(some_time, true /* raster */);
  }

  void RequestNewLayerTreeFrameSink() override {
    delegate_->add_request();
    LayerTreeView::RequestNewLayerTreeFrameSink();
  }

  void DidInitializeLayerTreeFrameSink() override {
    LayerTreeView::DidInitializeLayerTreeFrameSink();
    delegate_->add_success();
    if (delegate_->num_successes() == expected_successes_) {
      EXPECT_EQ(delegate_->num_requests(), expected_requests_);
      EndTest();
    } else {
      // Post the synchronous composite task so that it is not called
      // reentrantly as a part of RequestNewLayerTreeFrameSink.
      blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &LayerTreeViewWithFrameSinkTracking::SynchronousComposite,
              base::Unretained(this)));
    }
  }

  void DidFailToInitializeLayerTreeFrameSink() override {
    LayerTreeView::DidFailToInitializeLayerTreeFrameSink();
    delegate_->add_failure();
    if (delegate_->num_requests() == expected_requests_) {
      EXPECT_EQ(delegate_->num_successes(), expected_successes_);
      EndTest();
      return;
    }
  }

  void SetUp(int expected_successes,
             int num_tries,
             FailureMode failure_mode,
             base::RunLoop* run_loop) {
    run_loop_ = run_loop;
    failure_mode_ = failure_mode;
    expected_successes_ = expected_successes;
    switch (failure_mode_) {
      case NO_FAILURE:
        expected_requests_ = expected_successes;
        break;
      case BIND_CONTEXT_FAILURE:
      case GPU_CHANNEL_FAILURE:
        expected_requests_ = num_tries * std::max(1, expected_successes);
        break;
    }
  }

  void EndTest() { run_loop_->Quit(); }

 private:
  FakeLayerTreeViewDelegate* delegate_;
  base::RunLoop* run_loop_ = nullptr;
  int expected_successes_ = 0;
  int expected_requests_ = 0;
  FailureMode failure_mode_ = NO_FAILURE;

  DISALLOW_COPY_AND_ASSIGN(LayerTreeViewWithFrameSinkTracking);
};

class LayerTreeViewWithFrameSinkTrackingTest : public testing::Test {
 public:
  LayerTreeViewWithFrameSinkTrackingTest()
      : layer_tree_view_(
            &layer_tree_view_delegate_,
            blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
            /*compositor_thread=*/nullptr,
            &test_task_graph_runner_,
            &fake_thread_scheduler_) {
    cc::LayerTreeSettings settings;
    settings.single_thread_proxy_scheduler = false;
    layer_tree_view_.Initialize(settings,
                                std::make_unique<cc::TestUkmRecorderFactory>());
  }

  void RunTest(int expected_successes, FailureMode failure_mode) {
    layer_tree_view_delegate_.Reset();
    // 6 is just an artibrary "large" number to show it keeps trying.
    const int kTries = 6;
    // If it should fail, then it will fail every attempt, otherwise it fails
    // until the last attempt.
    int tries_before_success = kTries - (expected_successes ? 1 : 0);
    switch (failure_mode) {
      case NO_FAILURE:
        layer_tree_view_delegate_.set_num_failures_before_success(0);
        layer_tree_view_delegate_.set_num_requests_before_success(0);
        break;
      case BIND_CONTEXT_FAILURE:
        layer_tree_view_delegate_.set_num_failures_before_success(
            tries_before_success);
        layer_tree_view_delegate_.set_num_requests_before_success(0);
        break;
      case GPU_CHANNEL_FAILURE:
        layer_tree_view_delegate_.set_num_failures_before_success(0);
        layer_tree_view_delegate_.set_num_requests_before_success(
            tries_before_success);
        break;
    }
    base::RunLoop run_loop;
    layer_tree_view_.SetUp(expected_successes, kTries, failure_mode, &run_loop);
    layer_tree_view_.SetVisible(true);
    blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LayerTreeViewWithFrameSinkTracking::SynchronousComposite,
            base::Unretained(&layer_tree_view_)));
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  cc::TestTaskGraphRunner test_task_graph_runner_;
  blink::scheduler::WebFakeThreadScheduler fake_thread_scheduler_;
  FakeLayerTreeViewDelegate layer_tree_view_delegate_;
  LayerTreeViewWithFrameSinkTracking layer_tree_view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayerTreeViewWithFrameSinkTrackingTest);
};

TEST_F(LayerTreeViewWithFrameSinkTrackingTest, SucceedOnce) {
  RunTest(1, NO_FAILURE);
}

TEST_F(LayerTreeViewWithFrameSinkTrackingTest, SucceedOnce_AfterNullChannel) {
  RunTest(1, GPU_CHANNEL_FAILURE);
}

TEST_F(LayerTreeViewWithFrameSinkTrackingTest, SucceedOnce_AfterLostContext) {
  RunTest(1, BIND_CONTEXT_FAILURE);
}

TEST_F(LayerTreeViewWithFrameSinkTrackingTest, SucceedTwice) {
  RunTest(2, NO_FAILURE);
}

TEST_F(LayerTreeViewWithFrameSinkTrackingTest, SucceedTwice_AfterNullChannel) {
  RunTest(2, GPU_CHANNEL_FAILURE);
}

TEST_F(LayerTreeViewWithFrameSinkTrackingTest, SucceedTwice_AfterLostContext) {
  RunTest(2, BIND_CONTEXT_FAILURE);
}

TEST_F(LayerTreeViewWithFrameSinkTrackingTest, FailWithNullChannel) {
  RunTest(0, GPU_CHANNEL_FAILURE);
}

TEST_F(LayerTreeViewWithFrameSinkTrackingTest, FailWithLostContext) {
  RunTest(0, BIND_CONTEXT_FAILURE);
}

class VisibilityTestLayerTreeView : public LayerTreeView {
 public:
  VisibilityTestLayerTreeView(
      StubLayerTreeViewDelegate* delegate,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_thread,
      cc::TaskGraphRunner* task_graph_runner,
      blink::scheduler::WebThreadScheduler* scheduler)
      : LayerTreeView(delegate,
                      std::move(main_thread),
                      std::move(compositor_thread),
                      task_graph_runner,
                      scheduler) {}

  void RequestNewLayerTreeFrameSink() override {
    LayerTreeView::RequestNewLayerTreeFrameSink();
    num_requests_sent_++;
    if (run_loop_)
      run_loop_->Quit();
  }

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }
  int num_requests_sent() { return num_requests_sent_; }

 private:
  int num_requests_sent_ = 0;
  base::RunLoop* run_loop_;
};

TEST(LayerTreeViewTest, VisibilityTest) {
  // Test that LayerTreeView does not retry FrameSink request while
  // invisible.

  base::test::TaskEnvironment task_environment;

  cc::TestTaskGraphRunner test_task_graph_runner;
  blink::scheduler::WebFakeThreadScheduler fake_thread_scheduler;
  // Synchronously callback with null FrameSink.
  StubLayerTreeViewDelegate layer_tree_view_delegate;
  VisibilityTestLayerTreeView layer_tree_view(
      &layer_tree_view_delegate,
      blink::scheduler::GetSingleThreadTaskRunnerForTesting(),
      /*compositor_thread=*/nullptr, &test_task_graph_runner,
      &fake_thread_scheduler);

  layer_tree_view.Initialize(cc::LayerTreeSettings(),
                             std::make_unique<cc::TestUkmRecorderFactory>());

  {
    // Make one request and stop immediately while invisible.
    base::RunLoop run_loop;
    layer_tree_view.set_run_loop(&run_loop);
    layer_tree_view.SetVisible(false);
    layer_tree_view.RequestNewLayerTreeFrameSink();
    run_loop.Run();
    layer_tree_view.set_run_loop(nullptr);
    EXPECT_EQ(1, layer_tree_view.num_requests_sent());
  }

  {
    // Make sure there are no more requests.
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
    EXPECT_EQ(1, layer_tree_view.num_requests_sent());
  }

  {
    // Becoming visible retries request.
    base::RunLoop run_loop;
    layer_tree_view.set_run_loop(&run_loop);
    layer_tree_view.SetVisible(true);
    run_loop.Run();
    layer_tree_view.set_run_loop(nullptr);
    EXPECT_EQ(2, layer_tree_view.num_requests_sent());
  }
}

}  // namespace
}  // namespace content
