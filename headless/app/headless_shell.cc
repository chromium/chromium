// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/app/headless_shell.h"

#include <memory>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "headless/app/headless_command_handler.h"
#include "headless/app/headless_shell_command_line.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_web_contents.h"
#include "net/base/filename_util.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "components/os_crypt/os_crypt_switches.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "components/crash/core/app/crash_switches.h"  // nogncheck
#include "components/crash/core/app/run_as_crashpad_handler_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if defined(HEADLESS_USE_POLICY)
#include "headless/lib/browser/policy/headless_mode_policy.h"
#endif

#if defined(HEADLESS_ENABLE_COMMANDS)
#include "headless/app/headless_command_handler.h"
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

}  // namespace

HeadlessShell::HeadlessShell() = default;
HeadlessShell::~HeadlessShell() = default;

void HeadlessShell::OnBrowserStart(HeadlessBrowser* browser) {
  browser_ = browser;

#if defined(HEADLESS_USE_POLICY)
  if (policy::HeadlessModePolicy::IsHeadlessDisabled(
          static_cast<HeadlessBrowserImpl*>(browser)->GetPrefs())) {
    LOG(ERROR) << "Headless mode is disabled by policy.";
    ShutdownSoon();
    return;
  }
#endif

  HeadlessBrowserContext::Builder context_builder =
      browser_->CreateBrowserContextBuilder();

  // Retrieve the locale set by InitApplicationLocale() in
  // headless_content_main_delegate.cc in a way that is free of side-effects.
  context_builder.SetAcceptLanguage(base::i18n::GetConfiguredLocale());

  // Create browser  context and set it as the default. The default browser
  // context is used by the Target.createTarget() DevTools command when no other
  // context is given.
  browser_context_ = context_builder.Build();
  browser_->SetDefaultBrowserContext(browser_context_);

  // If no explicit URL is present navigate to about:blank unless we're being
  // driven by a debugger.
  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();
  if (args.empty() && !IsRemoteDebuggingEnabled())
    args.push_back(kAboutBlank);

  if (args.empty()) {
    return;
  }

  GURL target_url = ConvertArgumentToURL(args.front());

  // If driven by a debugger just open the target page and
  // leave expecting the debugger will do what they need.
  if (IsRemoteDebuggingEnabled()) {
    HeadlessWebContents::Builder builder(
        browser_context_->CreateWebContentsBuilder());
    HeadlessWebContents* web_contents =
        builder.SetInitialURL(target_url).Build();
    if (!web_contents) {
      LOG(ERROR) << "Navigation to " << target_url << " failed.";
      ShutdownSoon();
    }
    return;
  }

  // Otherwise instantiate headless shell command handler that will
  // execute the commands against the target page.
#if defined(HEADLESS_ENABLE_COMMANDS)
  GURL handler_url = HeadlessCommandHandler::GetHandlerUrl();
  HeadlessWebContents::Builder builder(
      browser_context_->CreateWebContentsBuilder());
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
      base::BindOnce(&HeadlessShell::ShutdownSoon, weak_factory_.GetWeakPtr()));
#endif
}

void HeadlessShell::ShutdownSoon() {
  browser_->BrowserMainThread()->PostTask(
      FROM_HERE,
      base::BindOnce(&HeadlessShell::Shutdown, weak_factory_.GetWeakPtr()));
}

void HeadlessShell::Shutdown() {
  browser_->Shutdown();
}

#if BUILDFLAG(IS_WIN)
int HeadlessShellMain(HINSTANCE instance,
                      sandbox::SandboxInterfaceInfo* sandbox_info) {
  base::CommandLine::Init(0, nullptr);
#if defined(HEADLESS_USE_CRASHPAD)
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ::switches::kProcessType);
  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    return crash_reporter::RunAsCrashpadHandler(
        *base::CommandLine::ForCurrentProcess(), base::FilePath(),
        ::switches::kProcessType, switches::kUserDataDir);
  }
#endif  // defined(HEADLESS_USE_CRASHPAD)
  RunChildProcessIfNeeded(instance, sandbox_info);
  HeadlessBrowser::Options::Builder builder(0, nullptr);
  builder.SetInstance(instance);
  builder.SetSandboxInfo(std::move(sandbox_info));
#else
int HeadlessShellMain(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);
  RunChildProcessIfNeeded(argc, argv);
  HeadlessBrowser::Options::Builder builder(argc, argv);
#endif  // BUILDFLAG(IS_WIN)

  base::CommandLine& command_line(*base::CommandLine::ForCurrentProcess());

#if BUILDFLAG(IS_MAC)
  command_line.AppendSwitch(os_crypt::switches::kUseMockKeychain);
#endif

#if BUILDFLAG(IS_FUCHSIA)
  // TODO(fuchsia): Remove this when GPU accelerated compositing is ready.
  command_line.AppendSwitch(::switches::kDisableGpu);
#endif

  if (command_line.GetArgs().size() > 1) {
    LOG(ERROR) << "Multiple targets are not supported.";
    return EXIT_FAILURE;
  }

  if (!HandleCommandLineSwitches(command_line, builder))
    return EXIT_FAILURE;

  HeadlessShell shell;

  return HeadlessBrowserMain(
      builder.Build(),
      base::BindOnce(&HeadlessShell::OnBrowserStart, base::Unretained(&shell)));
}

int HeadlessShellMain(const content::ContentMainParams& params) {
#if BUILDFLAG(IS_WIN)
  return HeadlessShellMain(params.instance, params.sandbox_info);
#else
  return HeadlessShellMain(params.argc, params.argv);
#endif
}

namespace {

int RunContentMain(
    HeadlessBrowser::Options options,
    base::OnceCallback<void(HeadlessBrowser*)> on_browser_start_callback) {
  content::ContentMainParams params(nullptr);
#if BUILDFLAG(IS_WIN)
  // Sandbox info has to be set and initialized.
  CHECK(options.sandbox_info);
  params.instance = options.instance;
  params.sandbox_info = std::move(options.sandbox_info);
#elif !BUILDFLAG(IS_ANDROID)
  params.argc = options.argc;
  params.argv = options.argv;
#endif

  auto browser = std::make_unique<HeadlessBrowserImpl>(
      std::move(on_browser_start_callback), std::move(options));
  HeadlessContentMainDelegate delegate(std::move(browser));
  params.delegate = &delegate;
  return content::ContentMain(std::move(params));
}

}  // namespace

#if BUILDFLAG(IS_WIN)
void RunChildProcessIfNeeded(HINSTANCE instance,
                             sandbox::SandboxInterfaceInfo* sandbox_info) {
  base::CommandLine::Init(0, nullptr);
  HeadlessBrowser::Options::Builder builder(0, nullptr);
  builder.SetInstance(instance);
  builder.SetSandboxInfo(std::move(sandbox_info));
#else
void RunChildProcessIfNeeded(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);
  HeadlessBrowser::Options::Builder builder(argc, argv);
#endif  // BUILDFLAG(IS_WIN)
  const base::CommandLine& command_line(
      *base::CommandLine::ForCurrentProcess());

  if (!command_line.HasSwitch(::switches::kProcessType))
    return;

  int rc = RunContentMain(builder.Build(),
                          base::OnceCallback<void(HeadlessBrowser*)>());

  // Note that exiting from here means that base::AtExitManager objects will not
  // have a chance to be destroyed (typically in main/WinMain).
  // Use TerminateCurrentProcessImmediately instead of exit to avoid shutdown
  // crashes and slowdowns on shutdown.
  base::Process::TerminateCurrentProcessImmediately(rc);
}

int HeadlessBrowserMain(
    HeadlessBrowser::Options options,
    base::OnceCallback<void(HeadlessBrowser*)> on_browser_start_callback) {
  DCHECK(!on_browser_start_callback.is_null());
#if DCHECK_IS_ON()
  // The browser can only be initialized once.
  static bool browser_was_initialized;
  DCHECK(!browser_was_initialized);
  browser_was_initialized = true;

  // Child processes should not end up here.
  DCHECK(!base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kProcessType));
#endif
  return RunContentMain(std::move(options),
                        std::move(on_browser_start_callback));
}

}  // namespace headless
