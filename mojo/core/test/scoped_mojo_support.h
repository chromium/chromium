// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_TEST_SCOPED_MOJO_SUPPORT_H_
#define MOJO_CORE_TEST_SCOPED_MOJO_SUPPORT_H_

#include "base/test/test_io_thread.h"
#include "mojo/core/embedder/scoped_ipc_support.h"

namespace mojo::core::test {

// Brings up and cleanly tears down a Mojo Core instance in the current process,
// including a dedicated IO thread and ScopedIPCSupport. In order for Mojo to
// have its features properly configured, this object must be constructed AFTER
// base::FeatureList initialization.
//
// Test suites should generally initialize and tear this down around each
// individual test. MojoTestSuiteBase does exactly that when used.
class ScopedMojoSupport {
 public:
  ScopedMojoSupport();
  ScopedMojoSupport(const ScopedMojoSupport&) = delete;
  ScopedMojoSupport& operator=(const ScopedMojoSupport&) = delete;
  ~ScopedMojoSupport();

 private:
  class CoreInstance;

  std::unique_ptr<CoreInstance> core_;
  base::TestIOThread test_io_thread_{base::TestIOThread::kAutoStart};
  mojo::core::ScopedIPCSupport ipc_support_{
      test_io_thread_.task_runner(),
      mojo::core::ScopedIPCSupport::ShutdownPolicy::CLEAN};
};

}  // namespace mojo::core::test

#endif  // MOJO_CORE_TEST_SCOPED_MOJO_SUPPORT_H_
