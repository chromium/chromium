// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/context_provider_for_test_v1.h"

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/sys/cpp/component_context.h>
#include <unistd.h>
#include <zircon/processargs.h>

#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestNameSwitch[] = "test-name";

fidl::InterfaceHandle<fuchsia::io::Directory> StartWebEngineForTestsInternal(
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        component_controller_request,
    const base::CommandLine& base_command_line) {
  DCHECK(base_command_line.argv()[0].empty()) << "Must use NO_PROGRAM.";

  base::CommandLine command_line = base_command_line;
  constexpr char const* kSwitchesToCopy[] = {"ozone-platform"};
  command_line.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                kSwitchesToCopy, std::size(kSwitchesToCopy));

  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url =
      "fuchsia-pkg://fuchsia.com/web_engine#meta/context_provider.cmx";
  // Add all switches and arguments, skipping the program.
  launch_info.arguments.emplace(std::vector<std::string>(
      command_line.argv().begin() + 1, command_line.argv().end()));

  fuchsia::io::DirectorySyncPtr web_engine_services_dir;
  launch_info.directory_request = web_engine_services_dir.NewRequest();

  fuchsia::sys::LauncherPtr launcher;
  base::ComponentContextForProcess()->svc()->Connect(launcher.NewRequest());
  launcher->CreateComponent(std::move(launch_info),
                            std::move(component_controller_request));

  // The WebEngine binary can take sufficiently long for blobfs to resolve that
  // tests using it may timeout as a result. Wait for the ContextProvider to
  // be responsive, by making a synchronous request to its service directory.
  fuchsia::io::NodeAttributes attributes{};
  zx_status_t stat{};
  zx_status_t status = web_engine_services_dir->GetAttr(&stat, &attributes);
  ZX_CHECK(status == ZX_OK, status);

  return web_engine_services_dir.Unbind();
}

}  // namespace

// static
ContextProviderForTest ContextProviderForTest::Create(
    const base::CommandLine& command_line) {
  ::fuchsia::sys::ComponentControllerPtr web_engine_controller;
  ::sys::ServiceDirectory web_engine_service_dir(StartWebEngineForTestsInternal(
      web_engine_controller.NewRequest(), command_line));
  ::fuchsia::web::ContextProviderPtr context_provider;
  zx_status_t status =
      web_engine_service_dir.Connect(context_provider.NewRequest());
  EXPECT_EQ(status, ZX_OK) << "Connect to ContextProvider";
  return ContextProviderForTest(std::move(web_engine_controller),
                                std::move(context_provider));
}

ContextProviderForTest::ContextProviderForTest(
    ContextProviderForTest&&) noexcept = default;
ContextProviderForTest& ContextProviderForTest::operator=(
    ContextProviderForTest&&) noexcept = default;
ContextProviderForTest::~ContextProviderForTest() = default;

ContextProviderForTest::ContextProviderForTest(
    ::fuchsia::sys::ComponentControllerPtr web_engine_controller,
    ::fuchsia::web::ContextProviderPtr context_provider)
    : web_engine_controller_(std::move(web_engine_controller)),
      context_provider_(std::move(context_provider)) {}

// static
ContextProviderForDebugTest ContextProviderForDebugTest::Create(
    const base::CommandLine& command_line) {
  // Add a switch to the WebEngine instance to distinguish it from other
  // instances that may be started by other tests.
  const std::string test_name =
      testing::UnitTest::GetInstance()->current_test_info()->name();
  base::CommandLine command_line_for_debug(command_line);
  command_line_for_debug.AppendSwitchASCII(kTestNameSwitch, test_name);

  auto context_provider =
      ContextProviderForTest::Create(command_line_for_debug);

  // Wait for the OnDirectoryReady event, which indicates that the component's
  // outgoing directory is available, including the "/debug" contents accessed
  // via the Hub.
  base::RunLoop directory_loop;
  context_provider.component_controller_ptr().events().OnDirectoryReady =
      [quit_loop = directory_loop.QuitClosure()]() { quit_loop.Run(); };
  directory_loop.Run();

  // Enumerate all entries in /hub/c/context_provider.cmx to find WebEngine
  // instance with |test_switch|.
  base::FileEnumerator file_enum(base::FilePath("/hub/c/context_provider.cmx"),
                                 false, base::FileEnumerator::DIRECTORIES);
  base::FilePath web_engine_path;

  const std::string test_switch =
      base::StrCat({"--", kTestNameSwitch, "=", test_name});
  std::string args;
  for (auto dir = file_enum.Next(); !dir.empty(); dir = file_enum.Next()) {
    if (!base::ReadFileToString(dir.Append("args"), &args)) {
      // WebEngine may shutdown while we are enumerating the directory, so
      // it's safe to ignore this error.
      continue;
    }

    if (args.find(test_switch) != std::string::npos) {
      // There should only one instance of WebEngine with |test_switch|.
      EXPECT_EQ(web_engine_path, base::FilePath());
      web_engine_path = std::move(dir);

      // Keep iterating to check that there are no other matching instances.
    }
  }

  // Check that we've found the WebEngine instance with |test_switch|.
  EXPECT_FALSE(web_engine_path.empty());

  ::sys::ServiceDirectory debug_service_directory(
      base::OpenDirectoryHandle(web_engine_path.Append("out/debug")));

  return ContextProviderForDebugTest(std::move(context_provider),
                                     std::move(debug_service_directory));
}

ContextProviderForDebugTest::ContextProviderForDebugTest(
    ContextProviderForDebugTest&&) noexcept = default;
ContextProviderForDebugTest& ContextProviderForDebugTest::operator=(
    ContextProviderForDebugTest&&) noexcept = default;
ContextProviderForDebugTest::~ContextProviderForDebugTest() = default;

void ContextProviderForDebugTest::ConnectToDebug(
    ::fidl::InterfaceRequest<::fuchsia::web::Debug> debug_request) {
  ASSERT_EQ(debug_service_directory_.Connect(std::move(debug_request)), ZX_OK);
}

ContextProviderForDebugTest::ContextProviderForDebugTest(
    ContextProviderForTest context_provider,
    ::sys::ServiceDirectory debug_service_directory)
    : context_provider_(std::move(context_provider)),
      debug_service_directory_(std::move(debug_service_directory)) {}
