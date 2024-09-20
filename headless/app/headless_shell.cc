// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/headless_shell.h"

#include <memory>

#include "base/base_switches.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/version_info/version_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_web_contents.h"
#include "headless/public/switches.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "components/os_crypt/sync/os_crypt_switches.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "components/crash/core/app/crash_switches.h"  // nogncheck
#include "components/crash/core/app/run_as_crashpad_handler_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if defined(HEADLESS_USE_POLICY)
#include "components/headless/policy/headless_mode_policy.h"  // nogncheck
#endif

#if defined(HEADLESS_ENABLE_COMMANDS)
#include "components/headless/command_handler/headless_command_handler.h"  // nogncheck
#endif

namespace headless {

namespace {

#if BUILDFLAG(IS_WIN)
const wchar_t kAboutBlank[] = L"about:blank";
#else
const char kAboutBlank[] = "about:blank";
#endif

GURL ConvertArgumentToURL(const base::CommandLine::StringType& arg) {
#if BUILDFLAG(IS_WIN)
  GURL url(base::WideToUTF8(arg));
#else
  GURL url(arg);
#endif
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(arg)));
}

// An application which implements a simple headless browser.
class HeadlessShell {
 public:
  HeadlessShell() = default;

  HeadlessShell(const HeadlessShell&) = delete;
  HeadlessShell& operator=(const HeadlessShell&) = delete;

  ~HeadlessShell() = default;

  void OnBrowserStart(HeadlessBrowser* browser);

 private:
#if defined(HEADLESS_ENABLE_COMMANDS)
  void OnProcessCommandsDone(HeadlessCommandHandler::Result result);
#endif
  void ShutdownSoon();
  void Shutdown();

  raw_ptr<HeadlessBrowser> browser_ = nullptr;
};

void HeadlessShell::OnBrowserStart(HeadlessBrowser* browser) {
  browser_ = browser;

#if defined(HEADLESS_USE_POLICY)
  if (HeadlessModePolicy::IsHeadlessModeDisabled(
          static_cast<HeadlessBrowserImpl*>(browser)->GetPrefs())) {
    LOG(ERROR) << "Headless mode is disallowed by the system admin.";
    ShutdownSoon();
    return;
  }
#endif

  HeadlessBrowserContext::Builder context_builder =
      browser_->CreateBrowserContextBuilder();

  // Create browser context and set it as the default. The default browser
  // context is used by the Target.createTarget() DevTools command when no other
  // context is given.
  HeadlessBrowserContext* browser_context = context_builder.Build();
  browser_->SetDefaultBrowserContext(browser_context);

  const bool devtools_enabled = static_cast<HeadlessBrowserImpl*>(browser)
                                    ->options()
                                    ->DevtoolsServerEnabled();

  const base::CommandLine& command_line =
      CHECK_DEREF(base::CommandLine::ForCurrentProcess());
  base::CommandLine::StringVector args = command_line.GetArgs();

  // Remove empty arguments sometimes left there by scripts to prevent weird
  // error messages.
  args.erase(
      std::remove(args.begin(), args.end(), base::CommandLine::StringType()),
      args.end());

  // If no explicit URL is present assume about:blank unless we're being
  // driven by a debugger.
  if (args.empty() && !devtools_enabled) {
    args.push_back(kAboutBlank);
  }

  if (args.empty()) {
    return;
  }

  GURL target_url = ConvertArgumentToURL(args.front());
  HeadlessWebContents::Builder builder(
      browser_context->CreateWebContentsBuilder());

  // Check for headless commands and instantiate headless command handler
  // that will execute the commands against the target page.
#if defined(HEADLESS_ENABLE_COMMANDS)
  if (HeadlessCommandHandler::HasHeadlessCommandSwitches(command_line)) {
    GURL handler_url = HeadlessCommandHandler::GetHandlerUrl();
    HeadlessWebContents* web_contents =
        builder.SetInitialURL(handler_url).Build();
    if (!web_contents) {
      LOG(ERROR) << "Navigation to " << handler_url << " failed.";
      ShutdownSoon();
      return;
    }

    HeadlessCommandHandler::ProcessCommands(
        HeadlessWebContentsImpl::From(web_contents)->web_contents(),
        std::move(target_url),
        base::BindOnce(&HeadlessShell::OnProcessCommandsDone,
                       base::Unretained(this)));
    return;
  }
#endif

  // Otherwise just open the target page.
  HeadlessWebContents* web_contents = builder.SetInitialURL(target_url).Build();
  if (!web_contents) {
    LOG(ERROR) << "Navigation to " << target_url << " failed.";
    ShutdownSoon();
  }
}

#if defined(HEADLESS_ENABLE_COMMANDS)
void HeadlessShell::OnProcessCommandsDone(
    HeadlessCommandHandler::Result result) {
  if (result != HeadlessCommandHandler::Result::kSuccess) {
    static_cast<HeadlessBrowserImpl*>(browser_)->ShutdownWithExitCode(
        static_cast<int>(result));
    return;
  }
  Shutdown();
}
#endif

void HeadlessShell::ShutdownSoon() {
  browser_->BrowserMainThread()->PostTask(
      FROM_HERE,
      base::BindOnce(&HeadlessShell::Shutdown, base::Unretained(this)));
}

void HeadlessShell::Shutdown() {
  browser_.ExtractAsDangling()->Shutdown();
}

void HeadlessChildMain(content::ContentMainParams params) {
  HeadlessContentMainDelegate delegate(nullptr);
  params.delegate = &delegate;
  int rc = content::ContentMain(std::move(params));

  // Note that exiting from here means that base::AtExitManager objects will not
  // have a chance to be destroyed (typically in main/WinMain).
  // Use TerminateCurrentProcessImmediately instead of exit to avoid shutdown
  // crashes and slowdowns on shutdown.
  base::Process::TerminateCurrentProcessImmediately(rc);
}

int HeadlessBrowserMain(content::ContentMainParams params) {
#if DCHECK_IS_ON()
  // The browser can only be initialized once.
  static bool browser_was_initialized;
  DCHECK(!browser_was_initialized);
  browser_was_initialized = true;

  // Child processes should not end up here.
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kProcessType));
#endif
#if defined(HEADLESS_ENABLE_COMMANDS)
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (HeadlessCommandHandler::HasHeadlessCommandSwitches(command_line)) {
    if (command_line.HasSwitch(::switches::kRemoteDebuggingPort) ||
        command_line.HasSwitch(::switches::kRemoteDebuggingPipe)) {
      LOG(ERROR)
          << "Headless commands are not compatible with remote debugging.";
      return EXIT_FAILURE;
    }
    command_line.AppendSwitch(switches::kDisableLazyLoading);
  }
#endif

  HeadlessShell shell;
  auto browser = std::make_unique<HeadlessBrowserImpl>(
      base::BindOnce(&HeadlessShell::OnBrowserStart, base::Unretained(&shell)));
  HeadlessContentMainDelegate delegate(std::move(browser));
  params.delegate = &delegate;
  return content::ContentMain(std::move(params));
}

}  // namespace

int HeadlessShellMain(content::ContentMainParams params) {
#if BUILDFLAG(IS_WIN)
  base::CommandLine::Init(0, nullptr);
#else
  base::CommandLine::Init(params.argc, params.argv);
#endif  // BUILDFLAG(IS_WIN)
  base::CommandLine& command_line(*base::CommandLine::ForCurrentProcess());
  std::string process_type =
      command_line.GetSwitchValueASCII(::switches::kProcessType);
#if defined(HEADLESS_USE_CRASHPAD)
  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    return crash_reporter::RunAsCrashpadHandler(
        *base::CommandLine::ForCurrentProcess(), base::FilePath(),
        ::switches::kProcessType, switches::kUserDataDir);
  }
#endif  // defined(HEADLESS_USE_CRASHPAD)

  if (!process_type.empty()) {
    HeadlessChildMain(std::move(params));
    NOTREACHED();
  }

#if BUILDFLAG(IS_MAC)
  command_line.AppendSwitch(os_crypt::switches::kUseMockKeychain);
#endif

#if BUILDFLAG(IS_FUCHSIA)
  // TODO(fuchsia): Remove this when GPU accelerated compositing is ready.
  command_line.AppendSwitch(::switches::kDisableGpu);
#endif

  if (command_line.HasSwitch(switches::kVersion)) {
    printf("%s %s\n", version_info::GetProductName().data(),
           version_info::GetVersionNumber().data());
    return EXIT_SUCCESS;
  }

  if (command_line.GetArgs().size() > 1) {
    LOG(ERROR) << "Multiple targets are not supported.";
    return EXIT_FAILURE;
  }

  return HeadlessBrowserMain(std::move(params));
}

}  // namespace headless
