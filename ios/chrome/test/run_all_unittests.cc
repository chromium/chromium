// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/threading/thread.h"
#include "ios/chrome/app/startup/ios_chrome_main.h"
#include "ios/chrome/test/ios_chrome_unit_test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

int main(int argc, char** argv) {
  IOSChromeMain::InitStartTime();
  IOSChromeUnitTestSuite test_suite(argc, argv);

  mojo::core::Init();

  base::Thread ipc_thread("IPC thread");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&IOSChromeUnitTestSuite::Run, base::Unretained(&test_suite)));
}
