// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_suite.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "third_party/ipcz/src/test/multinode_test.h"
#include "third_party/ipcz/src/test_buildflags.h"

#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
#include "third_party/ipcz/src/test/test_child_launcher.h"
#endif

int main(int argc, char** argv) {
#if BUILDFLAG(ENABLE_IPCZ_MULTIPROCESS_TESTS)
  ipcz::test::TestChildLauncher::Initialize(argc, argv);
#endif

  base::TestSuite test_suite(argc, argv);
  ipcz::test::RegisterMultinodeTests();

  mojo::core::Init(mojo::core::Configuration{
      .is_broker_process = !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kTestChildProcess),
  });
  base::TestIOThread test_io_thread(base::TestIOThread::kAutoStart);
  mojo::core::ScopedIPCSupport ipc_support(
      test_io_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
