// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// If you are looking to write a new browser test, you are probably looking for
// one of the already-implemented subclasses, e.g. `content::ContentBrowserTest`
// for tests that can run directly on top of content_shell,
// `InProcessBrowserTest` for tests that require `//chrome`-layer functionality,
// et cetera. See `//content/public/test/browser_test.h` for more information.
//
// `content::BrowserTestBase` is a base class that provides shared functionality
// across various types of browser tests. It is not intended for direct use in
// tests, as it does not actually define how to launch a browser, nor how to run
// a test in said browser.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_BASE_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/test/test_host_resolver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "storage/browser/quota/quota_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"

namespace base {
class CommandLine;
class FilePath;
class TimeDelta;
}  // namespace base

namespace ui {
class ScopedAnimationDurationScaleMode;
}

namespace content {
class BrowserMainParts;
class ContentMainDelegate;
class NoRendererCrashesAssertion;
class WebContents;

class BrowserTestBase : public ::testing::Test {
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

  // By default browser tests use hardcoded quota settings for consistency,
  // instead of dynamically based on available disk space. Tests can override
  // this if they want to use the production path.
  virtual bool UseProductionQuotaSettings();

  // This is invoked if the test receives SIGTERM or SIGSEGV.
  virtual void SignalRunTestOnMainThread(int signal) {}

  // Crash the Network Service process. Should only be called when
  // out-of-process Network Service is enabled. Re-applies any added host
  // resolver rules, though network tasks started before the call returns may
  // racily start before the rules have been re-applied.
  void SimulateNetworkServiceCrash();

  // Ignores all future NetworkService crashes that would be otherwise detected
  // and flagged by the AssertThatNetworkServiceDidNotCrash method.
  //
  // The IgnoreNetworkServiceCrashes method is useful in a test that plans to
  // trigger crashes. Note that calling IgnoreNetworkServiceCrashes is *not*
  // needed when triggering the crash via SimulateNetworkServiceCrash method.
  void IgnoreNetworkServiceCrashes();

  // Returns the host resolver being used for the tests. Subclasses might want
  // to configure it inside tests.
  net::RuleBasedHostResolverProc* host_resolver() {
    return test_host_resolver_ ? test_host_resolver_->host_resolver() : nullptr;
  }

  // Returns the NetworkServiceTest remote endpoint in this test fixture.
  mojo::Remote<network::mojom::NetworkServiceTest>& network_service_test() {
    return network_service_test_;
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

  // Returns a custom ContentMainDelegate to use for the test, or nullptr to use
  // the standard delegate. The returned object must live at least until
  // TearDownInProcessBrowserTextFixture is called.
  virtual ContentMainDelegate* GetOptionalContentMainDelegateOverride();

  // GTest assertions that the connection to `network_service_test_` did not get
  // dropped unexpectedly.
  void AssertThatNetworkServiceDidNotCrash();

  // Sets flag to allow host resolutions to reach the network. Must be called
  // before Setup() to take effect.
  void SetAllowNetworkAccessToHostResolutions();

  // Sets flag that will cause the network service's system DNS configuration to
  // be replaced with a basic, single-server configuration. This should improve
  // test reproducibility and consistency across platforms, at the cost of
  // disabling the platform-specific logic that handles system config changes.
  void SetReplaceSystemDnsConfig();

  // Sets DoH configuration for use during tests.
  void SetTestDohConfig(net::SecureDnsMode secure_dns_mode,
                        net::DnsOverHttpsConfig config);

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

  // Returns the HTTP embedded test server. Guaranteed to be non-NULL.
  const net::EmbeddedTestServer* embedded_test_server() const {
    return embedded_test_server_.get();
  }
  net::EmbeddedTestServer* embedded_test_server() {
    return embedded_test_server_.get();
  }

  bool set_up_called() { return set_up_called_; }

#if BUILDFLAG(IS_POSIX)
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

  // Call this before SetUp() to cause the test to generate pixel output. This
  // function also sets a fixed device scale factor which a test can change.
  // This is useful for consistent testing across devices with different
  // display densities.
  void EnablePixelOutput(float force_device_scale_factor = 1.f);

  // Call this before SetUp() to not use GL, but use software compositing
  // instead.
  void UseSoftwareCompositing();

  // Should be in PreRunTestOnMainThread, with the initial WebContents for the
  // main window. This allows the test harness to watch it for navigations so
  // that it can sync the host_resolver() rules to the out-of-process network
  // code necessary.
  void SetInitialWebContents(WebContents* web_contents);

 private:
#if BUILDFLAG(IS_ANDROID)
  // Android browser tests need to wait for async initialization in Java code.
  // This waits for those to complete before we can continue with the test.
  void WaitUntilJavaIsReady(base::OnceClosure quit_closure,
                            const base::TimeDelta& wait_retry_left);
#endif
  // Performs a bunch of setup, and then runs the browser test body.
  void ProxyRunTestOnMainThreadLoop();

  // Sets `initialized_network_process_` to false and calls
  // InitializeNetworkProcess(). Used when restarting the network service
  // process.
  void ForceInitializeNetworkProcess();

  // When using the network process, update the host resolver rules that were
  // added in SetUpOnMainThread.
  void InitializeNetworkProcess();

  // Captures |browser_main_parts_| and forwards the call to
  // CreatedBrowserMainParts().
  void CreatedBrowserMainPartsImpl(BrowserMainParts* browser_main_parts);

#if BUILDFLAG(IS_WIN)
  std::optional<base::ScopedPathOverride> system_temp_override_;
#endif

  // Embedded HTTP test server, cheap to create, started on demand.
  std::unique_ptr<net::EmbeddedTestServer> embedded_test_server_;

  // Host resolver used during tests.
  std::unique_ptr<TestHostResolver> test_host_resolver_;

  // When true, `InitializeNetworkProcess` will tell the network service to use
  // a dummy system DNS configuration.
  bool replace_system_dns_config_ = false;

  // DoH configuration used during tests. When it contains a value,
  // `InitializeNetworkProcess` will pass it to the network service.
  std::optional<std::pair<net::SecureDnsMode, net::DnsOverHttpsConfig>>
      test_doh_config_;

  // A field trial list that's used to support field trials activated prior to
  // browser start.
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

  // Expected exit code.
  int expected_exit_code_ = 0;

  // When true, the compositor will produce pixel output that can be read back
  // for pixel tests.
  bool enable_pixel_output_ = false;

  // When using EnablePixelOutput, the device scale factor is forced to an
  // explicit value to ensure consistent results. This value will be passed to
  // the --force-device-scale-factor flag in SetUp.
  float force_device_scale_factor_ = 0.f;

  // When verifying pixel output, animations are disabled to reduce flakiness.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode>
      disable_layer_animations_;
  gfx::AnimationTestApi::RenderModeResetter disable_rich_animations_;

  // When true, do compositing with the software backend instead of using GL.
  bool use_software_compositing_ = false;

  // Initial WebContents to watch for navigations during SetUpOnMainThread.
  base::WeakPtr<WebContents> initial_web_contents_;

  // Whether SetUp was called. This value is checked in the destructor of this
  // class to ensure that SetUp was called. If it's not called, the test will
  // not run and report a false positive result.
  bool set_up_called_ = false;

  std::unique_ptr<storage::QuotaSettings> quota_settings_;

  std::unique_ptr<NoRendererCrashesAssertion> no_renderer_crashes_assertion_;

  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test_;

  bool initialized_network_process_ = false;

  bool allow_network_access_to_host_resolutions_ = false;

  raw_ptr<BrowserMainParts, AcrossTasksDanglingUntriaged> browser_main_parts_ =
      nullptr;

#if BUILDFLAG(IS_POSIX)
  bool handle_sigterm_;
#endif
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_BASE_H_
