// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_switches_internal.h"
#include "content/ppapi_plugin/ppapi_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"

namespace content {

// Main function for starting the PPAPI broker process.
int PpapiBrokerMain(const MainFunctionParams& parameters) {
  const base::CommandLine& command_line = parameters.command_line;
  if (command_line.HasSwitch(switches::kPpapiStartupDialog))
    WaitForDebugger("PpapiBroker");

  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::PlatformThread::SetName("CrPPAPIBrokerMain");
  base::trace_event::TraceLog::GetInstance()->set_process_name(
      "PPAPI Broker Process");
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventPpapiBrokerProcessSortIndex);

  ChildProcess ppapi_broker_process;
  base::RunLoop run_loop;
  ppapi_broker_process.set_main_thread(new PpapiThread(
      run_loop.QuitClosure(), parameters.command_line, true /* Broker */));

  run_loop.Run();
  DVLOG(1) << "PpapiBrokerMain exiting";
  return 0;
}

}  // namespace content
