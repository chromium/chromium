// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_pump_type.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "dbus/bus.h"
#include "dbus/test_service.h"

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  TestTimeouts::Initialize();

  base::Thread dbus_thread("D-Bus Thread");
  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  CHECK(dbus_thread.StartWithOptions(std::move(thread_options)));

  dbus::TestService::Options options;
  options.dbus_task_runner = dbus_thread.task_runner();
  dbus::TestService* test_service = new dbus::TestService(options);
  CHECK(test_service->StartService());
  test_service->WaitUntilServiceIsStarted();
  CHECK(test_service->HasDBusThread());
}
