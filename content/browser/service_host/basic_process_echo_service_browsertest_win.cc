// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/service_host/utility_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"
#include "sandbox/policy/switches.h"
#include "services/test/echo/public/mojom/echo.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

constexpr char kLogFileSwitch[] = "basic-process-api-test-log-file";
constexpr char kLogCallSitesSwitch[] = "basic-process-api-test-log-call-sites";
constexpr char kAuditSwitch[] = "basic-process-api-test-audit";
constexpr base::FilePath::CharType kFixtureExecutable[] =
    FILE_PATH_LITERAL("basic_process_echo_service.exe");
constexpr base::FilePath::CharType kShimLibrary[] =
    FILE_PATH_LITERAL("apifw.dll");

// Shim log line shapes:
//   [<pid>] stub: <thunk>(<detail>)
//   [<pid>] stub: on-demand loaded <module>
//   [<pid>] stub-site: <thunk> <module>+<rva>;<module>+<rva>;...
//   stub-stack: <thunk> <address>;...  (unprefixed; only ApifwNotReached)
constexpr std::string_view kStubPrefix = "stub: ";
constexpr std::string_view kCallSitePrefix = "stub-site: ";
constexpr std::string_view kNotReachedPrefix = "stub-stack: ";
constexpr std::string_view kOnDemandLoadPrefix = "on-demand loaded ";
constexpr std::string_view kGetProcAddressThunk = "ApifwGetProcAddress";

struct AuditLog {
  std::set<std::string> pids;
  std::set<std::string> thunks;
  std::set<std::string> resolved_symbols;
  std::set<std::string> call_site_modules;
  std::set<std::string> on_demand_modules;
  std::vector<std::string> unparsed_lines;
  size_t call_sites = 0;
  size_t not_reached_stacks = 0;
};

AuditLog ParseAuditLog(std::string_view contents) {
  AuditLog audit;
  for (std::string_view line : base::SplitStringPiece(
           contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    // Tripwire stacks are emitted without the per-process prefix.
    if (line.starts_with(kNotReachedPrefix)) {
      ++audit.not_reached_stacks;
      continue;
    }

    const size_t pid_end =
        line.starts_with("[") ? line.find(']') : std::string_view::npos;
    if (pid_end == std::string_view::npos) {
      audit.unparsed_lines.emplace_back(line);
      continue;
    }
    audit.pids.emplace(line.substr(1, pid_end - 1));
    const std::string_view record =
        base::TrimWhitespaceASCII(line.substr(pid_end + 1), base::TRIM_LEADING);

    if (record.starts_with(kCallSitePrefix)) {
      const std::string_view payload = record.substr(kCallSitePrefix.size());
      const size_t stack_start = payload.find(' ');
      if (stack_start == std::string_view::npos) {
        audit.unparsed_lines.emplace_back(line);
        continue;
      }
      // Attribute the call to the module holding the immediate caller.
      const std::string_view stack = payload.substr(stack_start + 1);
      const std::string_view frame = stack.substr(0, stack.find(';'));
      const size_t rva_start = frame.find('+');
      if (rva_start == std::string_view::npos) {
        audit.unparsed_lines.emplace_back(line);
        continue;
      }
      audit.call_site_modules.emplace(frame.substr(0, rva_start));
      ++audit.call_sites;
      continue;
    }

    if (record.starts_with(kStubPrefix)) {
      const std::string_view payload = record.substr(kStubPrefix.size());
      if (payload.starts_with(kOnDemandLoadPrefix)) {
        audit.on_demand_modules.emplace(
            payload.substr(kOnDemandLoadPrefix.size()));
        continue;
      }
      const size_t detail_start = payload.find('(');
      const size_t detail_end = payload.rfind(')');
      if (detail_start == std::string_view::npos ||
          detail_end == std::string_view::npos || detail_end < detail_start) {
        audit.unparsed_lines.emplace_back(line);
        continue;
      }
      const std::string_view thunk = payload.substr(0, detail_start);
      audit.thunks.emplace(thunk);
      if (thunk == kGetProcAddressThunk && detail_end > detail_start + 1) {
        audit.resolved_symbols.emplace(
            payload.substr(detail_start + 1, detail_end - detail_start - 1));
      }
      continue;
    }

    audit.unparsed_lines.emplace_back(line);
  }
  return audit;
}

class EchoServiceClient final : public UtilityProcessHost::Client {
 public:
  EchoServiceClient(base::OnceCallback<void(base::ProcessId)> launch_callback,
                    base::OnceCallback<void(bool)> exit_callback)
      : launch_callback_(std::move(launch_callback)),
        exit_callback_(std::move(exit_callback)) {}

  void OnProcessLaunched(const base::Process& process) override {
    std::move(launch_callback_).Run(process.Pid());
  }

  void OnProcessTerminatedNormally() override {
    std::move(exit_callback_).Run(true);
  }

  void OnProcessCrashed(CrashType) override {
    std::move(exit_callback_).Run(false);
  }

 private:
  base::OnceCallback<void(base::ProcessId)> launch_callback_;
  base::OnceCallback<void(bool)> exit_callback_;
};

class BasicProcessEchoServiceBrowserTest : public ContentBrowserTest {};

// Launches one unsandboxed EchoService utility child from a copy of
// content_shell.exe whose imports are bound to the logging shim, then checks
// that the shim recorded that child and only that child.
IN_PROC_BROWSER_TEST_F(BasicProcessEchoServiceBrowserTest,
                       AuditsEchoServiceApiCalls) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  const base::FilePath output_dir =
      base::PathService::CheckedGet(base::DIR_EXE);
  const base::FilePath fixture_path = output_dir.Append(kFixtureExecutable);
  ASSERT_TRUE(base::PathExists(fixture_path))
      << "Missing " << fixture_path
      << ". Build //content/test:basic_process_echo_service_fixture.";
  ASSERT_TRUE(base::PathExists(output_dir.Append(kShimLibrary)));

  // Audit runs keep the log by naming it on the browser command line.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath log_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kLogFileSwitch);
  if (log_path.empty()) {
    log_path = temp_dir.GetPath().Append(FILE_PATH_LITERAL("api_calls.log"));
  }

  // The fixture is a copy of content_shell.exe, whose entry point hands the
  // child broker services because it is not a sandbox target. Passing
  // --no-sandbox keeps the child from pre-creating an alternate desktop for
  // sandboxed children it will never launch.
  std::vector<std::string> child_switches = {
      kAuditSwitch, kLogCallSitesSwitch, sandbox::policy::switches::kNoSandbox};

  mojo::Remote<echo::mojom::EchoService> echo;
  base::test::TestFuture<base::ProcessId> launch_future;
  base::test::TestFuture<bool> exit_future;
  auto client = std::make_unique<EchoServiceClient>(launch_future.GetCallback(),
                                                    exit_future.GetCallback());

  {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchPath(
        switches::kBrowserSubprocessPath, fixture_path);
    ASSERT_TRUE(UtilityProcessHost::Start(
        UtilityProcessHost::Options()
            .WithSandboxType(sandbox::mojom::Sandbox::kNoSandbox)
            .WithName(u"Basic Process EchoService test fixture")
            .WithMetricsName(echo::mojom::EchoService::Name_)
            .WithExtraCommandLineSwitches(std::move(child_switches))
            .WithExtraCommandLineSwitchKeyValues(
                {{kLogFileSwitch, base::WideToUTF8(log_path.value())}})
            .WithBoundServiceInterfaceOnChildProcess(
                echo.BindNewPipeAndPassReceiver())
            .Pass(),
        std::move(client)));
  }

  const base::ProcessId service_pid = launch_future.Take();
  ASSERT_NE(base::kNullProcessId, service_pid);

  constexpr char kPayload[] = "basic-process-test-fixture";
  base::test::TestFuture<const std::string&> echo_future;
  echo->EchoString(kPayload, echo_future.GetCallback());
  EXPECT_EQ(kPayload, echo_future.Get());

  echo->Quit();
  EXPECT_TRUE(exit_future.Get()) << "The EchoService child did not exit "
                                    "normally; see the audit log at "
                                 << log_path;

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(log_path, &contents))
      << "The shim wrote no audit log to " << log_path;
  const AuditLog audit = ParseAuditLog(contents);

  EXPECT_TRUE(audit.unparsed_lines.empty())
      << audit.unparsed_lines.size() << " unrecognized log lines, first: "
      << (audit.unparsed_lines.empty() ? std::string()
                                       : audit.unparsed_lines.front());
  EXPECT_EQ(0u, audit.not_reached_stacks)
      << "The child called an API routed to the ApifwNotReached tripwire.";
  EXPECT_EQ(std::set<std::string>{base::NumberToString(service_pid)},
            audit.pids)
      << "Only the audited EchoService child may write to the log.";
  EXPECT_FALSE(audit.thunks.empty());
  EXPECT_GT(audit.call_sites, 0u);
  EXPECT_EQ(std::set<std::string>{"basic_process_echo_service.exe"},
            audit.call_site_modules)
      << "Every logged call must come from the rewritten fixture image.";

  LOG(INFO) << "EchoService pid " << service_pid << " logged "
            << audit.thunks.size() << " distinct thunks, "
            << audit.resolved_symbols.size() << " resolved symbols, "
            << audit.on_demand_modules.size() << " on-demand module loads, and "
            << audit.call_sites << " call sites to " << log_path;
}

}  // namespace
}  // namespace content
