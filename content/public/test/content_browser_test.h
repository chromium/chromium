// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Provides a pre-defined browser test fixture for browser tests that run
// directly on top of `content_shell` (e.g. no functionality from //chrome, et
// cetera is needed), which is often the case for web platform features.
//
// Intended to be used in conjunction with //content/public/test/browser_test.h
// and one of the `IN_PROC_BROWSER_TEST_*` macros defined in that header; please
// see that header for more info about using those macros.
//
// Commonly overridden methods:
//
// - void SetUpCommandLine(base::CommandLine* command_line), e.g. to add
//   command-line flags to enable / configure the feature being tested.
//
// - void SetUpOnMainThread(), to run test set-up steps on the browser main
//   thread, e.g. installing hooks in the browser process for testing. Note that
//   this method cannot be used to configure renderer-side hooks, as renderers
//   spawn in separate processes. Most often, renderers are configured via
//   pre-existing IPCs or via command-line flags.
//
// As a hack, it *is* possible to use `--single-process`, which runs the
// renderer in the same process, but on a different thread. This is very
// strongly discouraged. :)

#ifndef CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_H_
#define CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/test/browser_test_base.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/memory/stack_allocated.h"
#include "base/test/scoped_path_override.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif

namespace content {
class Shell;

// Base class for browser tests which use content_shell.
class ContentBrowserTest : public BrowserTestBase {
 protected:
  ContentBrowserTest();
  ~ContentBrowserTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // BrowserTestBase:
  void PreRunTestOnMainThread() override;
  void PostRunTestOnMainThread() override;

 protected:
  // Creates a new window and loads about:blank.
  Shell* CreateBrowser();

  // Creates an off-the-record window and loads about:blank.
  Shell* CreateOffTheRecordBrowser();

  // Returns the window for the test.
  Shell* shell() const { return shell_; }

  // File path to test data, relative to DIR_SOURCE_ROOT.
  base::FilePath GetTestDataFilePath();

 private:
  raw_ptr<Shell, AcrossTasksDanglingUntriaged> shell_ = nullptr;

#if BUILDFLAG(IS_MAC)
  // On Mac, without the following autorelease pool, code which is directly
  // executed (as opposed to executed inside a message loop) would autorelease
  // objects into a higher-level pool. This pool is not recycled in-sync with
  // the message loops' pools and causes problems with code relying on
  // deallocation via an autorelease pool (such as browser window closure and
  // browser shutdown). To avoid this, the following pool is recycled after each
  // time code is directly executed.
  STACK_ALLOCATED_IGNORE("https://crbug.com/1424190")
  absl::optional<base::apple::ScopedNSAutoreleasePool> pool_;

  absl::optional<base::ScopedPathOverride> file_exe_override_;
#endif

  // Used to detect incorrect overriding of PreRunTestOnMainThread() with
  // missung call to base implementation.
  bool pre_run_test_executed_ = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_H_
