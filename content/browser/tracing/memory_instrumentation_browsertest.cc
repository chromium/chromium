// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/bind.h"
#include "base/run_loop.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Le;
using testing::Ge;
using testing::AllOf;
using memory_instrumentation::GlobalMemoryDump;
using memory_instrumentation::mojom::ProcessType;

namespace content {

class MemoryInstrumentationTest : public ContentBrowserTest {
 protected:
  void Navigate(Shell* shell) {
    EXPECT_TRUE(NavigateToURL(shell, GetTestUrl("", "title1.html")));
  }
};

uint64_t GetPrivateFootprintKb(ProcessType type,
                               const GlobalMemoryDump& global_dump,
                               base::ProcessId pid = base::kNullProcessId) {
  const GlobalMemoryDump::ProcessDump* target_dump = nullptr;
  for (const auto& dump : global_dump.process_dumps()) {
    if (dump.process_type() != type)
      continue;

    if (pid != base::kNullProcessId && pid != dump.pid())
      continue;

    EXPECT_FALSE(target_dump);
    target_dump = &dump;
  }
  EXPECT_TRUE(target_dump);
  return target_dump->os_dump().private_footprint_kb;
}

std::unique_ptr<GlobalMemoryDump> DoGlobalDump() {
  std::unique_ptr<GlobalMemoryDump> result = nullptr;
  base::RunLoop run_loop;
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestGlobalDump(
          {}, base::BindOnce(
                  [](base::Closure quit_closure,
                     std::unique_ptr<GlobalMemoryDump>* out_result,
                     bool success, std::unique_ptr<GlobalMemoryDump> result) {
                    EXPECT_TRUE(success);
                    *out_result = std::move(result);
                    std::move(quit_closure).Run();
                  },
                  run_loop.QuitClosure(), &result));
  run_loop.Run();
  return result;
}

// *SAN fake some sys calls we need meaning we never get dumps for the
// processes.
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(THREAD_SANITIZER)
#define MAYBE_PrivateFootprintComputation DISABLED_PrivateFootprintComputation
#else
#define MAYBE_PrivateFootprintComputation PrivateFootprintComputation
#endif

// Despite the location, this test is not tracing related.
// TODO(hjd): Move this once we have a resource_coordinator folder in browser.
IN_PROC_BROWSER_TEST_F(MemoryInstrumentationTest,
                       MAYBE_PrivateFootprintComputation) {
  Navigate(shell());

  // We have to pick a big size (>=64mb) to avoid an implementation detail of
  // malloc on MacOS which doesn't free or mark as reusable small allocations
  // after a free.
  const int64_t kAllocSize = 65 * 1024 * 1024;
  const int64_t kAllocSizeKb = kAllocSize / 1024;

  std::unique_ptr<GlobalMemoryDump> before_ptr = DoGlobalDump();

  std::unique_ptr<char[]> buffer = std::make_unique<char[]>(kAllocSize);
  memset(buffer.get(), 1, kAllocSize);
  volatile char* x = static_cast<volatile char*>(buffer.get());
  EXPECT_EQ(x[0] + x[kAllocSize - 1], 2);

  content::WebContents* web_contents = shell()->web_contents();
  base::ProcessId renderer_pid =
      web_contents->GetMainFrame()->GetProcess()->GetProcess().Pid();

  // Should allocate at least 4*10^6 / 1024 = 4000kb.
  EXPECT_TRUE(content::ExecuteScript(web_contents,
                                     "var a = Array(1000000).fill(1234);\n"));

  std::unique_ptr<GlobalMemoryDump> during_ptr = DoGlobalDump();

  buffer.reset();

  std::unique_ptr<GlobalMemoryDump> after_ptr = DoGlobalDump();

  int64_t before_kb = GetPrivateFootprintKb(ProcessType::BROWSER, *before_ptr);
  int64_t during_kb = GetPrivateFootprintKb(ProcessType::BROWSER, *during_ptr);
  int64_t after_kb = GetPrivateFootprintKb(ProcessType::BROWSER, *after_ptr);

  EXPECT_THAT(after_kb - before_kb,
              AllOf(Ge(-kAllocSizeKb / 10), Le(kAllocSizeKb / 10)));
  EXPECT_THAT(during_kb - before_kb,
              AllOf(Ge(kAllocSizeKb - 3000), Le(kAllocSizeKb + 3000)));
  EXPECT_THAT(during_kb - after_kb,
              AllOf(Ge(kAllocSizeKb - 3000), Le(kAllocSizeKb + 3000)));

  int64_t before_renderer_kb =
      GetPrivateFootprintKb(ProcessType::RENDERER, *before_ptr, renderer_pid);
  int64_t during_renderer_kb =
      GetPrivateFootprintKb(ProcessType::RENDERER, *during_ptr, renderer_pid);
  EXPECT_GE(during_renderer_kb - before_renderer_kb, 3000);
}

}  // namespace content
