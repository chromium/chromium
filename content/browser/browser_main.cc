// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main.h"

#include <memory>

#include "base/debug/alias.h"
#include "base/process/current_process.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/browser_main_runner_impl.h"
#include "content/common/content_constants_internal.h"

namespace content {

// Main routine for running as the Browser process.
int BrowserMain(MainFunctionParams parameters) {
  TRACE_EVENT_INSTANT0("startup", "BrowserMain", TRACE_EVENT_SCOPE_THREAD);

  base::CurrentProcess::GetInstance().SetProcessType(
      base::CurrentProcessType::PROCESS_BROWSER);
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventBrowserProcessSortIndex);

  std::unique_ptr<BrowserMainRunnerImpl> main_runner(
      BrowserMainRunnerImpl::Create());

  int exit_code = main_runner->Initialize(std::move(parameters));
  if (exit_code >= 0)
    return exit_code;

  exit_code = main_runner->Run();

  // Record the time shutdown started in convenient units. This can be compared
  // to times stored in places like ReportThreadHang() and
  // TaskAnnotator::RunTaskImpl() when analyzing hangs.
  const int64_t shutdown_time =
      base::TimeTicks::Now().since_origin().InSeconds();
  base::debug::Alias(&shutdown_time);

  main_runner->Shutdown();

  return exit_code;
}

}  // namespace content
