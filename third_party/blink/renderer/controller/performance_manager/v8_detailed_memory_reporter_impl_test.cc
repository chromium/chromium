// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_detailed_memory_reporter_impl.h"

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

class V8DetailedMemoryReporterImplTest : public SimTest {};

class V8DetailedMemoryReporterImplWorkerTest : public DedicatedWorkerTest {};

namespace {

class MemoryUsageChecker {
 public:
  enum class CallbackAction { kExitRunLoop, kNone };
  MemoryUsageChecker(size_t expected_isolate_count,
                     size_t expected_context_count)
      : expected_isolate_count_(expected_isolate_count),
        expected_context_count_(expected_context_count) {}

  void Callback(mojom::blink::PerProcessV8MemoryUsagePtr result) {
    EXPECT_EQ(expected_isolate_count_, result->isolates.size());
    size_t actual_context_count = 0;
    for (const auto& isolate : result->isolates) {
      for (const auto& entry : isolate->contexts) {
        // The memory usage of each context should be at least 1000000 bytes
        // because each context allocates a byte array of that length. Since
        // other objects are allocated during context initialization we can
        // only check the lower bound.
        EXPECT_LE(1000000u, entry->bytes_used);
        ++actual_context_count;
        if (entry->token.Is<DedicatedWorkerToken>()) {
          EXPECT_EQ(String("http://fake.url/"), entry->url);
        } else {
          EXPECT_FALSE(entry->url);
        }
      }
    }
    EXPECT_EQ(expected_context_count_, actual_context_count);
    called_ = true;
    loop_.Quit();
  }

  void Run() { loop_.Run(); }

  bool IsCalled() { return called_; }

 private:
  size_t expected_isolate_count_;
  size_t expected_context_count_;
  bool called_ = false;
  base::RunLoop loop_;
};

class CanvasMemoryUsageChecker {
 public:
  CanvasMemoryUsageChecker(size_t canvas_width, size_t canvas_height)
      : canvas_width_(canvas_width), canvas_height_(canvas_height) {}

  void Callback(mojom::blink::PerProcessV8MemoryUsagePtr result) {
    const size_t kMinBytesPerPixel = 1;
    size_t actual_context_count = 0;
    for (const auto& isolate : result->isolates) {
      for (const auto& entry : isolate->contexts) {
        EXPECT_LE(canvas_width_ * canvas_height_ * kMinBytesPerPixel,
                  entry->bytes_used);
        ++actual_context_count;
      }
    }
    EXPECT_EQ(1u, actual_context_count);
    called_ = true;
    loop_.Quit();
  }
  void Run() { loop_.Run(); }
  bool IsCalled() { return called_; }

 private:
  size_t canvas_width_ = 0;
  size_t canvas_height_ = 0;
  bool called_ = false;
  base::RunLoop loop_;
};

}  // anonymous namespace

TEST_F(V8DetailedMemoryReporterImplTest, GetV8MemoryUsage) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://example.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(R"HTML(
      <script>
        window.onload = function () {
          globalThis.root = {
            array: new Uint8Array(1000000)
          };
          console.log("main loaded");
        }
      </script>
      <body>
        <iframe src='https://example.com/subframe.html'></iframe>
      </body>)HTML");

  test::RunPendingTasks();

  child_frame_resource.Complete(R"HTML(
      <script>
        window.onload = function () {
          globalThis.root = {
            array: new Uint8Array(1000000)
          };
          console.log("iframe loaded");
        }
      </script>
      <body>
      </body>)HTML");

  test::RunPendingTasks();
  // Ensure that main frame and subframe are loaded before measuring memory
  // usage.
  EXPECT_TRUE(ConsoleMessages().Contains("main loaded"));
  EXPECT_TRUE(ConsoleMessages().Contains("iframe loaded"));

  V8DetailedMemoryReporterImpl reporter;
  // We expect to see the main isolate with two contexts corresponding to
  // the main page and the iframe.
  size_t expected_isolate_count = 1;
  size_t expected_context_count = 2;
  MemoryUsageChecker checker(expected_isolate_count, expected_context_count);
  reporter.GetV8MemoryUsage(
      V8DetailedMemoryReporterImpl::Mode::EAGER,
      WTF::BindOnce(&MemoryUsageChecker::Callback, WTF::Unretained(&checker)));

  checker.Run();

  EXPECT_TRUE(checker.IsCalled());
}

TEST_F(V8DetailedMemoryReporterImplWorkerTest, GetV8MemoryUsage) {
  base::RunLoop loop;
  const String source_code = R"JS(
    globalThis.root = {
      array: new Uint8Array(1000000)
    };)JS";
  StartWorker();
  EvaluateClassicScript(source_code);
  WaitUntilWorkerIsRunning();
  V8DetailedMemoryReporterImpl reporter;
  // We expect to see two isolates: the main isolate and the worker isolate.
  // Only the worker isolate has a context. The main isolate is empty because
  // DedicatedWorkerTest does not set it up.
  size_t expected_isolate_count = 2;
  size_t expected_context_count = 1;
  MemoryUsageChecker checker(expected_isolate_count, expected_context_count);
  reporter.GetV8MemoryUsage(
      V8DetailedMemoryReporterImpl::Mode::EAGER,
      WTF::BindOnce(&MemoryUsageChecker::Callback, WTF::Unretained(&checker)));
  checker.Run();
  EXPECT_TRUE(checker.IsCalled());
}

TEST_F(V8DetailedMemoryReporterImplTest, CanvasMemoryUsage) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  // CanvasPerformanceMonitor::CurrentTaskDrawsToContext() which is invoked from
  // JS below expects to be run from a task as it adds itself to as a
  // TaskTimeObserver that is cleared when the task is finished. Not doing so
  // violates CanvasPerformanceMonitor consistency.
  Window()
      .GetTaskRunner(TaskType::kNetworking)
      ->PostTask(FROM_HERE, base::BindLambdaForTesting([&main_resource] {
                   main_resource.Complete(R"HTML(
      <script>
        window.onload = function () {
          let canvas = document.getElementById('test');
          let ctx = canvas.getContext("2d");
          ctx.moveTo(0, 0);
          ctx.lineTo(200, 100);
          ctx.stroke();
          console.log("main loaded");
        }
      </script>
      <body>
        <canvas id="test" width="10" height="10"></canvas>
      </body>)HTML");
                 }));

  test::RunPendingTasks();

  // Ensure that main frame and subframe are loaded before measuring memory
  // usage.
  ASSERT_TRUE(ConsoleMessages().Contains("main loaded"));

  V8DetailedMemoryReporterImpl reporter;
  CanvasMemoryUsageChecker checker(10, 10);
  reporter.GetV8MemoryUsage(V8DetailedMemoryReporterImpl::Mode::EAGER,
                            WTF::BindOnce(&CanvasMemoryUsageChecker::Callback,
                                          WTF::Unretained(&checker)));
  checker.Run();
  EXPECT_TRUE(checker.IsCalled());
}

}  // namespace blink
