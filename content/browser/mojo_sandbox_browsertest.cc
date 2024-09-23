// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/sandbox.h"
#include "sandbox/policy/sandbox_type.h"
#include "sandbox/policy/switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {
namespace {

const std::string kTestMessage = "My hovercraft is full of eels!";

class MojoSandboxTest : public ContentBrowserTest {
 public:
  MojoSandboxTest() = default;

  MojoSandboxTest(const MojoSandboxTest&) = delete;
  MojoSandboxTest& operator=(const MojoSandboxTest&) = delete;

  using BeforeStartCallback = base::OnceCallback<void(UtilityProcessHost*)>;

  void StartProcess(BeforeStartCallback callback = BeforeStartCallback()) {
    host_ = std::make_unique<UtilityProcessHost>();
    host_->SetMetricsName("mojo_sandbox_test_process");
    if (callback)
      std::move(callback).Run(host_.get());
    ASSERT_TRUE(host_->Start());
  }

  mojo::Remote<mojom::TestService> BindTestService() {
    mojo::Remote<mojom::TestService> test_service;
    host_->GetChildProcess()->BindServiceInterface(
        test_service.BindNewPipeAndPassReceiver());
    return test_service;
  }

  void TearDownOnMainThread() override { host_.reset(); }

 protected:
  std::unique_ptr<UtilityProcessHost> host_;
};

// Ensures that a read-only shared memory region can be created within a
// sandboxed process.
IN_PROC_BROWSER_TEST_F(MojoSandboxTest, SubprocessReadOnlySharedMemoryRegion) {
  StartProcess();
  mojo::Remote<mojom::TestService> test_service = BindTestService();

  bool got_response = false;
  base::RunLoop run_loop;
  test_service.set_disconnect_handler(run_loop.QuitClosure());
  test_service->CreateReadOnlySharedMemoryRegion(
      kTestMessage,
      base::BindLambdaForTesting([&](base::ReadOnlySharedMemoryRegion region) {
        got_response = true;
        ASSERT_TRUE(region.IsValid());
        base::ReadOnlySharedMemoryMapping mapping = region.Map();
        ASSERT_TRUE(mapping.IsValid());
        auto span = mapping.GetMemoryAsSpan<const char>();
        EXPECT_EQ(kTestMessage, std::string_view(span.data(), span.size()));
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(got_response);
}

// Ensures that a writable shared memory region can be created within a
// sandboxed process.
IN_PROC_BROWSER_TEST_F(MojoSandboxTest, SubprocessWritableSharedMemoryRegion) {
  StartProcess();
  mojo::Remote<mojom::TestService> test_service = BindTestService();

  bool got_response = false;
  base::RunLoop run_loop;
  test_service.set_disconnect_handler(run_loop.QuitClosure());
  test_service->CreateWritableSharedMemoryRegion(
      kTestMessage,
      base::BindLambdaForTesting([&](base::WritableSharedMemoryRegion region) {
        got_response = true;
        ASSERT_TRUE(region.IsValid());
        base::WritableSharedMemoryMapping mapping = region.Map();
        ASSERT_TRUE(mapping.IsValid());
        auto span = mapping.GetMemoryAsSpan<const char>();
        EXPECT_EQ(kTestMessage, std::string_view(span.data(), span.size()));
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(got_response);
}

// Ensures that an unsafe shared memory region can be created within a
// sandboxed process.
IN_PROC_BROWSER_TEST_F(MojoSandboxTest, SubprocessUnsafeSharedMemoryRegion) {
  StartProcess();
  mojo::Remote<mojom::TestService> test_service = BindTestService();

  bool got_response = false;
  base::RunLoop run_loop;
  test_service.set_disconnect_handler(run_loop.QuitClosure());
  test_service->CreateUnsafeSharedMemoryRegion(
      kTestMessage,
      base::BindLambdaForTesting([&](base::UnsafeSharedMemoryRegion region) {
        got_response = true;
        ASSERT_TRUE(region.IsValid());
        base::WritableSharedMemoryMapping mapping = region.Map();
        ASSERT_TRUE(mapping.IsValid());
        auto span = mapping.GetMemoryAsSpan<const char>();
        EXPECT_EQ(kTestMessage, std::string_view(span.data(), span.size()));
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_TRUE(got_response);
}

// Test for sandbox::policy::IsProcessSandboxed().
IN_PROC_BROWSER_TEST_F(MojoSandboxTest, IsProcessSandboxed) {
  StartProcess();
  mojo::Remote<mojom::TestService> test_service = BindTestService();

  // The browser should not be considered sandboxed.
  EXPECT_FALSE(sandbox::policy::Sandbox::IsProcessSandboxed());

  std::optional<bool> maybe_is_sandboxed;
  base::RunLoop run_loop;
  test_service.set_disconnect_handler(run_loop.QuitClosure());
  test_service->IsProcessSandboxed(
      base::BindLambdaForTesting([&](bool is_sandboxed) {
        maybe_is_sandboxed = is_sandboxed;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(maybe_is_sandboxed.has_value());
  EXPECT_TRUE(maybe_is_sandboxed.value());
}

// TODO(crbug.com/40126761): There is currently no way to know whether a
// child process is sandboxed or not on Fuchsia.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_NotIsProcessSandboxed DISABLED_NotIsProcessSandboxed
#else
#define MAYBE_NotIsProcessSandboxed NotIsProcessSandboxed
#endif
IN_PROC_BROWSER_TEST_F(MojoSandboxTest, MAYBE_NotIsProcessSandboxed) {
  StartProcess(base::BindOnce([](UtilityProcessHost* host) {
    host->SetSandboxType(sandbox::mojom::Sandbox::kNoSandbox);
  }));
  mojo::Remote<mojom::TestService> test_service = BindTestService();

  // The browser should not be considered sandboxed.
  EXPECT_FALSE(sandbox::policy::Sandbox::IsProcessSandboxed());

  std::optional<bool> maybe_is_sandboxed;
  base::RunLoop run_loop;
  test_service.set_disconnect_handler(run_loop.QuitClosure());
  test_service->IsProcessSandboxed(
      base::BindLambdaForTesting([&](bool is_sandboxed) {
        maybe_is_sandboxed = is_sandboxed;
        run_loop.Quit();
      }));
  run_loop.Run();
  ASSERT_TRUE(maybe_is_sandboxed.has_value());
  // If the content_browsertests is launched with --no-sandbox, that will
  // get passed down to the browser and all child processes. In that case,
  // IsProcessSandboxed() will report true, per the API.
  bool no_sandbox = base::CommandLine::ForCurrentProcess()->HasSwitch(
      sandbox::policy::switches::kNoSandbox);
  EXPECT_EQ(no_sandbox, maybe_is_sandboxed.value());
}

}  //  namespace
}  //  namespace content
