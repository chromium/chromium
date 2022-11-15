// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browser_main.h"

#include <memory>

#include "base/process/current_process.h"
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

  main_runner->Shutdown();

  return exit_code;
}

}  // namespace content
