// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_process_host.h"

#include <string.h>

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "services/test/echo/public/mojom/echo.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

constexpr char kTestUrl[] = "https://foo.bar";

using ServiceProcessHostBrowserTest = ContentBrowserTest;

class EchoServiceProcessObserver : public ServiceProcessHost::Observer {
 public:
  EchoServiceProcessObserver() { ServiceProcessHost::AddObserver(this); }

  EchoServiceProcessObserver(const EchoServiceProcessObserver&) = delete;
  EchoServiceProcessObserver& operator=(const EchoServiceProcessObserver&) =
      delete;

  ~EchoServiceProcessObserver() override {
    ServiceProcessHost::RemoveObserver(this);
  }

  void WaitForLaunch() { launch_loop_.Run(); }
  void WaitForDeath() { death_loop_.Run(); }
  void WaitForCrash() { crash_loop_.Run(); }

  // Valid after WaitForLaunch.
  base::ProcessId pid() const { return process_.Pid(); }

 private:
  // ServiceProcessHost::Observer:
  void OnServiceProcessLaunched(const ServiceProcessInfo& info) override {
    if (info.IsService<echo::mojom::EchoService>()) {
      process_ = info.GetProcess().Duplicate();
      launch_loop_.Quit();
    }
  }

  void OnServiceProcessTerminatedNormally(
      const ServiceProcessInfo& info) override {
    if (info.IsService<echo::mojom::EchoService>())
      death_loop_.Quit();
  }

  void OnServiceProcessCrashed(const ServiceProcessInfo& info) override {
    if (info.IsService<echo::mojom::EchoService>()) {
      ASSERT_EQ(info.site(), GURL(kTestUrl));
      crash_loop_.Quit();
    }
  }

  base::RunLoop launch_loop_;
  base::RunLoop death_loop_;
  base::RunLoop crash_loop_;
  base::Process process_;
};

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, Launch) {
  EchoServiceProcessObserver observer;
  base::ProcessId pid_from_callback = base::kNullProcessId;
  base::RunLoop pid_loop;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>(
      ServiceProcessHost::Options()
          .WithProcessCallback(
              base::BindLambdaForTesting([&](const base::Process& process) {
                pid_from_callback = process.Pid();
                pid_loop.Quit();
              }))
          .Pass());
  observer.WaitForLaunch();
  pid_loop.Run();
  EXPECT_EQ(pid_from_callback, observer.pid());
  EXPECT_NE(base::kNullProcessId, pid_from_callback);

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

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, AllMessagesReceived) {
  // Verifies that messages sent right before disconnection are always received
  // and dispatched by the service before it self-terminates.
  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>();

  const size_t kBufferSize = 256;
  const std::string kMessages[] = {
      "I thought we were having steamed clams.",
      "D'oh, no! I said steamed hams. That's what I call hamburgers.",
      "You call hamburgers, \"steamed hams?\"",
      "Yes. It's a regional dialect."};
  auto region = base::UnsafeSharedMemoryRegion::Create(kBufferSize);
  base::WritableSharedMemoryMapping mapping = region.Map();
  memset(mapping.memory(), 0, kBufferSize);

  // Send several messages, since it helps to verify a lack of raciness between
  // service-side message dispatch and service termination.
  for (const auto& message : kMessages) {
    ASSERT_LE(message.size(), kBufferSize);
    echo_service->EchoStringToSharedMemory(message, region.Duplicate());
  }
  echo_service.reset();
  observer.WaitForDeath();

  const std::string& kLastMessage = kMessages[std::size(kMessages) - 1];
  EXPECT_EQ(0,
            memcmp(mapping.memory(), kLastMessage.data(), kLastMessage.size()));
}

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, ObserveCrash) {
  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>(
      ServiceProcessHost::Options().WithSite(GURL(kTestUrl)).Pass());
  observer.WaitForLaunch();
  echo_service->Crash();
  observer.WaitForCrash();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, IdleTimeout) {
  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>();

  base::RunLoop wait_for_idle_loop;
  constexpr auto kTimeout = base::Seconds(1);
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
