// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_BASE_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_BASE_H_

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/metrics/field_trial.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace content {
class BrowserMainParts;
class WebContents;

class BrowserTestBase : public testing::Test {
 public:
  BrowserTestBase();
  ~BrowserTestBase() override;

  // Configures everything for an in process browser test (e.g. thread pool,
  // etc.) by invoking ContentMain (or manually on OS_ANDROID). As such all
  // single-threaded initialization must be done before this step.
  //
  // ContentMain then ends up invoking RunTestOnMainThreadLoop with browser
  // threads already running.
  void SetUp() override;

  // Restores state configured in SetUp.
  void TearDown() override;

  // Override this to add any custom setup code that needs to be done on the
  // main thread after the browser is created and just before calling
  // RunTestOnMainThread().
  virtual void SetUpOnMainThread() {}

  // Override this to add any custom teardown code that needs to be done on the
  // main thread right after RunTestOnMainThread().
  virtual void TearDownOnMainThread() {}

  // Override this to add command line flags specific to your test.
  virtual void SetUpCommandLine(base::CommandLine* command_line) {}

  // Override this to disallow accesses to be production-compatible.
  virtual bool AllowFileAccessFromFiles();

  // Crash the Network Service process. Should only be called when
  // out-of-process Network Service is enabled. Re-applies any added host
  // resolver rules, though network tasks started before the call returns may
  // racily start before the rules have been re-applied.
  void SimulateNetworkServiceCrash();

  // Returns the host resolver being used for the tests. Subclasses might want
  // to configure it inside tests.
  net::RuleBasedHostResolverProc* host_resolver() {
    return test_host_resolver_->host_resolver();
  }

 protected:
  // We need these special methods because SetUp is the bottom of the stack
  // that winds up calling your test method, so it is not always an option
  // to do what you want by overriding it and calling the superclass version.
  //
  // Override this for things you would normally override SetUp for. It will be
  // called before your individual test fixture method is run, but after most
  // of the overhead initialization has occured.
  virtual void SetUpInProcessBrowserTestFixture() {}

  // Override this for things you would normally override TearDown for.
  virtual void TearDownInProcessBrowserTestFixture() {}

  // Called after the BrowserMainParts have been created, and before
  // PreEarlyInitialization() has been called.
  virtual void CreatedBrowserMainParts(BrowserMainParts* browser_main_parts) {}

  // This is invoked from main after browser_init/browser_main have completed.
  // This prepares for the test by creating a new browser and doing any other
  // initialization.
  // This is meant to be inherited only by the test harness.
  virtual void PreRunTestOnMainThread() = 0;

  // Override this rather than TestBody.
  // Note this is internally called by the browser test macros.
  virtual void RunTestOnMainThread() = 0;

  // This is invoked from main after RunTestOnMainThread has run, to give the
  // harness a chance for post-test cleanup.
  // This is meant to be inherited only by the test harness.
  virtual void PostRunTestOnMainThread() = 0;

  // Sets expected browser exit code, in case it's different than 0 (success).
  void set_expected_exit_code(int code) { expected_exit_code_ = code; }

  const net::SpawnedTestServer* spawned_test_server() const {
    return spawned_test_server_.get();
  }
  net::SpawnedTestServer* spawned_test_server() {
    return spawned_test_server_.get();
  }

  // Returns the embedded test server. Guaranteed to be non-NULL.
  const net::EmbeddedTestServer* embedded_test_server() const {
    return embedded_test_server_.get();
  }
  net::EmbeddedTestServer* embedded_test_server() {
    return embedded_test_server_.get();
  }

#if defined(OS_POSIX)
  // This is only needed by a test that raises SIGTERM to ensure that a specific
  // codepath is taken.
  void DisableSIGTERMHandling() {
    handle_sigterm_ = false;
  }
#endif

  // This function is meant only for classes that directly derive from this
  // class to construct the test server in their constructor. They might need to
  // call this after setting up the paths. Actual test cases should never call
  // this.
  // |test_server_base| is the path, relative to src, to give to the test HTTP
  // server.
  void CreateTestServer(const base::FilePath& test_server_base);

  // When the test is running in --single-process mode, runs the given task on
  // the in-process renderer thread. A nested run loop is run until it
  // returns.
  void PostTaskToInProcessRendererAndWait(base::OnceClosure task);

  // Call this before SetUp() to cause the test to generate pixel output.
  void EnablePixelOutput();

  // Call this before SetUp() to not use GL, but use software compositing
  // instead.
  void UseSoftwareCompositing();

  // Returns true if the test will be using GL acceleration via a software GL.
  bool UsingSoftwareGL() const;

  // Should be in PreRunTestOnMainThread, with the initial WebContents for the
  // main window. This allows the test harness to watch it for navigations so
  // that it can sync the host_resolver() rules to the out-of-process network
  // code necessary.
  void SetInitialWebContents(WebContents* web_contents);

 private:
#if defined(OS_ANDROID)
  // Android browser tests need to wait for async initialization in Java code.
  // This waits for those to complete before we can continue with the test.
  void WaitUntilJavaIsReady(base::OnceClosure quit_closure);
#endif
  // Performs a bunch of setup, and then runs the browser test body.
  void ProxyRunTestOnMainThreadLoop();

  // When using the network process, update the host resolver rules that were
  // added in SetUpOnMainThread.
  void InitializeNetworkProcess();

  // Testing server, started on demand.
  std::unique_ptr<net::SpawnedTestServer> spawned_test_server_;

  // Embedded test server, cheap to create, started on demand.
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server_;

  // Host resolver used during tests.
  std::unique_ptr<TestHostResolver> test_host_resolver_;

  // A field trial list that's used to support field trials activated prior to
  // browser start.
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  // Expected exit code (default is 0).
  int expected_exit_code_;

  // When true, the compositor will produce pixel output that can be read back
  // for pixel tests.
  bool enable_pixel_output_;

  // When true, do compositing with the software backend instead of using GL.
  bool use_software_compositing_;

  // Initial WebContents to watch for navigations during SetUpOnMainThread.
  WebContents* initial_web_contents_ = nullptr;

  // Whether SetUp was called. This value is checked in the destructor of this
  // class to ensure that SetUp was called. If it's not called, the test will
  // not run and report a false positive result.
  bool set_up_called_;

  std::unique_ptr<NoRendererCrashesAssertion> no_renderer_crashes_assertion_;

  bool initialized_network_process_ = false;

#if defined(OS_POSIX)
  bool handle_sigterm_;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_BASE_H_
