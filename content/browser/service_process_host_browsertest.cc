// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/service_process_host.h"

#include <string.h>

#include <array>

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

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/browser/service_process_host_passkeys.h"
#endif

namespace content {

#if BUILDFLAG(IS_WIN)
using LoadStatus = echo::mojom::EchoService::LoadStatus;
namespace {
// This is used as the module name to load, and to make the DLL filename.
constexpr const wchar_t* kEchoPreloadLibrary = L"echo_preload_library";

// DLL path relative to current executable.
base::FilePath GetDllPath(std::wstring mod_name) {
  mod_name.append(L".dll");
  auto exe_dir = base::PathService::CheckedGet(base::BasePathKey::DIR_EXE);
  return exe_dir.Append(mod_name);
}
}  // namespace
#endif  // BUILDFLAG(IS_WIN)

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
  const auto kMessages = std::to_array<std::string>({
      "I thought we were having steamed clams.",
      "D'oh, no! I said steamed hams. That's what I call hamburgers.",
      "You call hamburgers, \"steamed hams?\"",
      "Yes. It's a regional dialect.",
  });
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

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, PreloadLibraryNotSet) {
  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>(
      ServiceProcessHost::Options().Pass());
  observer.WaitForLaunch();

  base::RunLoop loop;
  echo_service->LoadNativeLibrary(
      GetDllPath(kEchoPreloadLibrary), /*call_sec32_delayload=*/false,
      base::BindLambdaForTesting([&](LoadStatus status, uint32_t result) {
        EXPECT_EQ(LoadStatus::kFailedLoadLibrary, status);
        EXPECT_EQ(DWORD{ERROR_ACCESS_DENIED}, result);
        loop.Quit();
      }));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, PreloadLibraryPreloaded) {
  std::vector<base::FilePath> preloads;
  preloads.push_back(GetDllPath(kEchoPreloadLibrary));

  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>(
      ServiceProcessHost::Options()
          .WithPreloadedLibraries(
              preloads, ServiceProcessHostPreloadLibraries::GetPassKey())
          .Pass());
  observer.WaitForLaunch();

  base::RunLoop loop;
  echo_service->LoadNativeLibrary(
      GetDllPath(kEchoPreloadLibrary),
      /*call_sec32_delayload=*/true,
      base::BindLambdaForTesting([&](LoadStatus status, uint32_t result) {
        EXPECT_EQ(LoadStatus::kSuccess, status);
        EXPECT_EQ(0u, result);
        loop.Quit();
      }));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, PreloadLibraryMultiple) {
  std::vector<base::FilePath> preloads;
  // dbghelp is a placeholder - it will likely be loaded already - this test is
  // validating that multiple libraries can be sent into the child.
  preloads.push_back(GetDllPath(L"dbghelp"));
  preloads.push_back(GetDllPath(kEchoPreloadLibrary));

  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>(
      ServiceProcessHost::Options()
          .WithPreloadedLibraries(
              preloads, ServiceProcessHostPreloadLibraries::GetPassKey())
          .Pass());
  observer.WaitForLaunch();

  base::RunLoop loop;
  echo_service->LoadNativeLibrary(
      GetDllPath(kEchoPreloadLibrary), /*call_sec32_delayload=*/false,
      base::BindLambdaForTesting([&](LoadStatus status, uint32_t result) {
        EXPECT_EQ(LoadStatus::kSuccess, status);
        EXPECT_EQ(0u, result);
        loop.Quit();
      }));
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, PreloadLibraryModName) {
  std::vector<base::FilePath> preloads;
  preloads.push_back(GetDllPath(kEchoPreloadLibrary));

  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>(
      ServiceProcessHost::Options()
          .WithPreloadedLibraries(
              preloads, ServiceProcessHostPreloadLibraries::GetPassKey())
          .Pass());
  observer.WaitForLaunch();

  base::RunLoop loop;
  // Once preloaded can people simply provide the module name?
  echo_service->LoadNativeLibrary(
      base::FilePath(kEchoPreloadLibrary), /*call_sec32_delayload=*/false,
      base::BindLambdaForTesting([&](LoadStatus status, uint32_t result) {
        EXPECT_EQ(LoadStatus::kSuccess, status);
        EXPECT_EQ(0u, result);
        loop.Quit();
      }));
  loop.Run();
}

// This test causes a CHECK in the child at startup.
IN_PROC_BROWSER_TEST_F(ServiceProcessHostBrowserTest, PreloadLibraryBadPath) {
  std::vector<base::FilePath> preloads;
  preloads.push_back(GetDllPath(L"this-is-not-a-library"));

  EchoServiceProcessObserver observer;
  auto echo_service = ServiceProcessHost::Launch<echo::mojom::EchoService>(
      ServiceProcessHost::Options()
          .WithSite(GURL(kTestUrl))  // For WaitForCrash().
          .WithPreloadedLibraries(
              preloads, ServiceProcessHostPreloadLibraries::GetPassKey())
          .Pass());
  observer.WaitForLaunch();
  observer.WaitForCrash();
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
