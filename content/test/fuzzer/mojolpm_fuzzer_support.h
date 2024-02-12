// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FUZZER_MOJOLPM_FUZZER_SUPPORT_H_
#define CONTENT_TEST_FUZZER_MOJOLPM_FUZZER_SUPPORT_H_

#include "base/at_exit.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"

namespace content {
namespace mojolpm {
// Global environment needed to run the interface being tested.
//
// This will be created once, before fuzzing starts, and will be shared between
// all testcases. It is created on the main thread.
//
// This file contains several base types that divide MojoLPM fuzzer
// implementations within //content into two kinds - those that can reuse the
// BrowserTaskEnvironment between multiple testcases, and those that require a
// new BrowserTaskEnvironment for every testcase (usually this happens when
// reusing existing test harness code).

// At a minimum, we should always be able to set up the command line, i18n and
// mojo, and create the thread on which the fuzzer will be run. We want to avoid
// (as much as is reasonable) any state being preserved between testcases.
class FuzzerEnvironment {
 public:
  FuzzerEnvironment(int argc, const char* const* argv);
  virtual ~FuzzerEnvironment();

  inline scoped_refptr<base::SequencedTaskRunner> fuzzer_task_runner() {
    return fuzzer_thread_.task_runner();
  }

 private:
  bool command_line_initialized_;
  base::AtExitManager at_exit_manager_;
  base::Thread fuzzer_thread_;

  mojo::internal::ScopedSuppressValidationErrorLoggingForTests
      validation_error_suppressor_;
  mojo::internal::SerializationWarningObserverForTesting
      serialization_error_suppressor_;

  TestContentClientInitializer content_client_initializer_;
};

// If we can also safely re-use a single BrowserTaskEnvironment and the
// TestContentClientInitializer between testcases, then prefer to use this case
// class instead.
class FuzzerEnvironmentWithTaskEnvironment : public FuzzerEnvironment {
 public:
  FuzzerEnvironmentWithTaskEnvironment(int argc, const char* const* argv);
  ~FuzzerEnvironmentWithTaskEnvironment() override;

 private:
  BrowserTaskEnvironment task_environment_;
};

// Fuzzers which need RenderViewHost/RenderFrameHost can use this adapter to
// reuse the existing test harness.
class RenderViewHostTestHarnessAdapter : public RenderViewHostTestHarness {
 public:
  RenderViewHostTestHarnessAdapter();
  ~RenderViewHostTestHarnessAdapter() override;

  // The UI thread only exists in between the calls to SetUp() and TearDown(),
  // so these need to be called on the main thread (ie. from the fuzzer
  // Testcase constructor and destructor, respectively).
  void SetUp() override;
  void TearDown() override;

  BrowserTaskEnvironment* task_environment();
  BrowserContext* browser_context();

 private:
  void TestBody() override {}
};
}  // namespace mojolpm
}  // namespace content

#endif  // CONTENT_TEST_FUZZER_MOJOLPM_FUZZER_SUPPORT_H_
