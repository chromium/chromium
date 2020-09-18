// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_detailed_memory_reporter_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
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
        // Each context allocates an array of 1000000u elements, thus 4000000u
        // is a lower bound of the memory usage on any platform. We cannot make
        // this check more strict without making the test fragile.
        EXPECT_LT(4000000u, entry->bytes_used);
        ++actual_context_count;
      }
    }
    EXPECT_EQ(expected_context_count_, actual_context_count);
    called_ = true;
    test::ExitRunLoop();
  }
  bool IsCalled() { return called_; }

 private:
  size_t expected_isolate_count_;
  size_t expected_context_count_;
  bool called_ = false;
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
          globalThis.array = new Array(1000000).fill(0);
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
          globalThis.array = new Array(1000000).fill(0);
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
      WTF::Bind(&MemoryUsageChecker::Callback, WTF::Unretained(&checker)));

  test::EnterRunLoop();

  EXPECT_TRUE(checker.IsCalled());
}

TEST_F(V8DetailedMemoryReporterImplWorkerTest, GetV8MemoryUsage) {
  const String source_code = "globalThis.array = new Array(1000000).fill(0);";
  StartWorker(source_code);
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
      WTF::Bind(&MemoryUsageChecker::Callback, WTF::Unretained(&checker)));
  test::EnterRunLoop();
  EXPECT_TRUE(checker.IsCalled());
}

}  // namespace blink
