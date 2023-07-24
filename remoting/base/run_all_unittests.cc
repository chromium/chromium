// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/threading/thread.h"
#include "build/chromeos_buildflags.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "remoting/base/chromeos_remoting_test_suite.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

int main(int argc, char** argv) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  remoting::ChromeOSRemotingTestSuite test_suite(argc, argv);
#else
  base::TestSuite test_suite(argc, argv);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::Thread ipc_thread("IPC thread");
  ipc_thread.StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));

  const auto* cmd = base::CommandLine::ForCurrentProcess();
  // We just assume that the main test process is a mojo broker, and all child
  // processes are non-brokers, since by default, the process sending
  // invitations needs to be a broker, while the process accepting invitations
  // needs to be a non-broker.
  bool is_broker_process = mojo::core::IsMojoIpczEnabled() &&
                           !cmd->HasSwitch(switches::kTestChildProcess);
  mojo::core::Init({.is_broker_process = is_broker_process});
  mojo::core::ScopedIPCSupport ipc_support(
      ipc_thread.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN);

  return base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
