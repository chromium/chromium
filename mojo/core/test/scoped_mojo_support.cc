// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/core/test/scoped_mojo_support.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/test_io_thread.h"
#include "mojo/core/embedder/configuration.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/core/embedder/scoped_ipc_support.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/core/test/test_support_impl.h"
#include "mojo/core/test/test_switches.h"
#include "mojo/public/tests/test_support_private.h"

namespace mojo::core::test {

namespace {

// Command-line switch to disable internal Mojo Channel capability
// advertisement, used to test skew between client versions.
const char kDisableAllCapabilities[] = "disable-all-capabilities";

class TestSupportInitializer {
 public:
  TestSupportInitializer() {
    mojo::test::TestSupport::Init(new mojo::core::test::TestSupportImpl());
  }
};

}  // namespace

class ScopedMojoSupport::CoreInstance {
 public:
  CoreInstance() {
    mojo::core::Configuration mojo_config;

    // A relatively low limit to make it easier to test behavior at the limit.
    mojo_config.max_message_num_bytes =
        mojo::core::test::MojoTestBase::kMaxMessageSizeInTests;
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kTestChildProcess) ||
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            test_switches::kMojoIsBroker)) {
      mojo_config.is_broker_process = true;
    }

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            kDisableAllCapabilities)) {
      mojo_config.dont_advertise_capabilities = true;
    }

    mojo::core::InitFeatures();
    mojo::core::Init(mojo_config);

    static TestSupportInitializer initializer;
  }

  ~CoreInstance() { mojo::core::ShutDown(); }
};

ScopedMojoSupport::ScopedMojoSupport()
    : core_(std::make_unique<CoreInstance>()) {
  // IO thread initialization can race to modify globals which other base object
  // initializations (e.g. TaskEnvironment) might touch on the main thread as a
  // side effect of any test that might run. Ensure the thread is fully started
  // before we proceed. See https://crbug.com/1364731.
  base::WaitableEvent io_thread_initialized;
  test_io_thread_.PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                             io_thread_initialized.Signal();
                           }));
  io_thread_initialized.Wait();
}

ScopedMojoSupport::~ScopedMojoSupport() = default;

}  // namespace mojo::core::test
