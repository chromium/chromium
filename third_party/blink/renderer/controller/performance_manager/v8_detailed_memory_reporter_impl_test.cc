// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/performance_manager/v8_detailed_memory_reporter_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/sim/sim_compositor.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

class V8DetailedMemoryReporterImplTest : public SimTest {};

namespace {

class MemoryUsageChecker {
 public:
  void Callback(mojom::blink::PerProcessV8MemoryUsagePtr result) {
    EXPECT_EQ(1u, result->isolates.size());
    EXPECT_EQ(2u, result->isolates[0]->contexts.size());
    for (const auto& entry : result->isolates[0]->contexts) {
      EXPECT_LT(4000000u, entry->bytes_used);
    }
    called_ = true;
  }
  bool IsCalled() { return called_; }

 private:
  bool called_ = false;
};

}  // anonymous namespace

TEST_F(V8DetailedMemoryReporterImplTest, GetV8MemoryUsage) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest child_frame_resource("https://example.com/subframe.html",
                                  "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(String::Format(R"HTML(
      <script>
        window.onload = function () {
          globalThis.array = new Array(1000000).fill(0);
          console.log("main loaded");
        }
      </script>
      <body>
        <iframe src='https://example.com/subframe.html'></iframe>
      </body>)HTML"));

  test::RunPendingTasks();

  child_frame_resource.Complete(String::Format(R"HTML(
      <script>
        window.onload = function () {
          globalThis.array = new Array(1000000).fill(0);
          console.log("iframe loaded");
        }
      </script>
      <body>
      </body>)HTML"));

  test::RunPendingTasks();

  // Ensure that main frame and subframe are loaded before measuring memory
  // usage.
  EXPECT_TRUE(ConsoleMessages().Contains("main loaded"));
  EXPECT_TRUE(ConsoleMessages().Contains("iframe loaded"));

  V8DetailedMemoryReporterImpl reporter;
  MemoryUsageChecker checker;
  reporter.GetV8MemoryUsage(
      V8DetailedMemoryReporterImpl::Mode::EAGER,
      WTF::Bind(&MemoryUsageChecker::Callback, WTF::Unretained(&checker)));

  test::RunPendingTasks();

  EXPECT_TRUE(checker.IsCalled());
}
}  // namespace blink
