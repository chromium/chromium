// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/service_process_host.h"
#include "content/public/test/content_browser_test.h"
#include "services/test/echo/public/mojom/echo.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ServiceProcessHostBrowserTest = ContentBrowserTest;

class EchoServiceProcessObserver : public ServiceProcessHost::Observer {
 public:
  EchoServiceProcessObserver() { ServiceProcessHost::AddObserver(this); }

  ~EchoServiceProcessObserver() override {
    ServiceProcessHost::RemoveObserver(this);
  }

  void WaitForLaunch() { launch_loop_.Run(); }
  void WaitForDeath() { death_loop_.Run(); }
  void WaitForCrash() { crash_loop_.Run(); }

 private:
  // ServiceProcessHost::Observer:
  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override {
    if (info.IsService<echo::mojom::EchoService>())
      launch_loop_.Quit();
  }

  void OnServiceProcessTerminatedNormally(
      const ServiceProcessInfo& info) override {
    if (info.IsService<echo::mojom::EchoService>())
      death_loop_.Quit();
  }

  void OnServiceProcessCrashed(const ServiceProcessInfo& info) override {
    if (info.IsService<echo::mojom::EchoService>())
      crash_loop_.Quit();
  }

  base::RunLoop launch_loop_;
  base::RunLoop death_loop_;
  base::RunLoop crash_loop_;

  DISALLOW_COPY_AND_ASSIGN(EchoServiceProcessObserver);
};

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, Launch) {
  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>();
  observer.WaitForLaunch();

  const std::string kTestString =
      "Aurora borealis! At this time of year? At this time of day? "
      "In this part of the country? Localized entirely within your kitchen?";
  base::RunLoop loop;
  echo_service->EchoString(
      kTestString,
      base::BindLambdaForTesting([&](const std::string& echoed_input) {
        EXPECT_EQ(kTestString, echoed_input);
        loop.Quit();
      }));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, LocalDisconnectQuits) {
  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>();
  observer.WaitForLaunch();
  echo_service.reset();
  observer.WaitForDeath();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, RemoteDisconnectQuits) {
  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>();
  observer.WaitForLaunch();
  echo_service->Quit();
  observer.WaitForDeath();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, ObserveCrash) {
  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>();
  observer.WaitForLaunch();
  echo_service->Crash();
  observer.WaitForCrash();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, IdleTimeout) {
  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>();

  base::RunLoop wait_for_idle_loop;
  constexpr auto kTimeout = base::TimeDelta::FromSeconds(1);
  echo_service.set_idle_handler(kTimeout, base::BindLambdaForTesting([&] {
                                  wait_for_idle_loop.Quit();
                                  echo_service.reset();
                                }));

  // Send a message and wait for the reply. Once the message is sent we should
  // observe at least |kTimeout| time elapsing before the RunLoop quits, because
  // the service process must wait at least that long to report itself as idle.
  base::ElapsedTimer timer;
  const std::string kTestString =
      "Yes, and you call them steamed hams despite the fact that they are "
      "obviously grilled.";
  echo_service->EchoString(
      kTestString,
      base::BindLambdaForTesting([&](const std::string& echoed_input) {
        EXPECT_EQ(kTestString, echoed_input);
      }));
  wait_for_idle_loop.Run();
  EXPECT_GE(timer.Elapsed(), kTimeout);

  // And since the idle handler resets |echo_service|, we should imminently see
  // normal service process termination.
  observer.WaitForDeath();
}

}  // namespace content
