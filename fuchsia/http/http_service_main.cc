// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/sys/cpp/component_context.h>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "fuchsia/http/http_service_impl.h"

int main(int argc, char** argv) {
  // Instantiate various global structures.
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("HTTP Service");
  base::CommandLine::Init(argc, argv);
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  base::AtExitManager exit_manager;

  // Bind the parent-supplied OutgoingDirectory-request to a directory and
  // publish the HTTP service into it.
  sys::OutgoingDirectory* outgoing_directory =
      base::fuchsia::ComponentContextForCurrentProcess()->outgoing().get();
  HttpServiceImpl http_service;
  base::fuchsia::ScopedServiceBinding<::fuchsia::net::oldhttp::HttpService>
      binding(outgoing_directory, &http_service);

  base::RunLoop run_loop;

  // The main thread loop will be terminated when there are no more clients
  // connected to this service. The system service manager will restart the
  // service on demand as needed.
  binding.SetOnLastClientCallback(
      base::BindOnce(&base::RunLoop::Quit, base::Unretained(&run_loop)));
  run_loop.Run();

  return 0;
}
