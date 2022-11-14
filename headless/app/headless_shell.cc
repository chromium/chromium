// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <cstdio>
#include <memory>
#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/rtl.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/switches.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "headless/app/headless_shell.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "headless/public/headless_devtools_target.h"
#include "net/base/filename_util.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/http/http_util.h"
#include "net/proxy_resolution/proxy_config.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include "components/crash/core/app/crash_switches.h"  // nogncheck
#include "components/crash/core/app/run_as_crashpad_handler_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "components/os_crypt/os_crypt_switches.h"  // nogncheck
#endif

#if defined(HEADLESS_USE_POLICY)
#include "headless/lib/browser/policy/headless_mode_policy.h"
#endif

namespace headless {

namespace {

// By default listen to incoming DevTools connections on localhost.
const char kUseLocalHostForDevToolsHttpServer[] = "localhost";
// Default file name for screenshot. Can be overriden by "--screenshot" switch.
const char kDefaultScreenshotFileName[] = "screenshot.png";
// Default file name for pdf. Can be overriden by "--print-to-pdf" switch.
const char kDefaultPDFFileName[] = "output.pdf";

bool ParseWindowSize(const std::string& window_size,
                     gfx::Size* parsed_window_size) {
  int width = 0;
  int height = 0;
  if (sscanf(window_size.c_str(), "%d%*[x,]%d", &width, &height) >= 2 &&
      width >= 0 && height >= 0) {
    parsed_window_size->set_width(width);
    parsed_window_size->set_height(height);
    return true;
  }
  return false;
}

bool ParseFontRenderHinting(
    const std::string& font_render_hinting_string,
    gfx::FontRenderParams::Hinting* font_render_hinting) {
  if (font_render_hinting_string == "max") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_MAX;
  } else if (font_render_hinting_string == "full") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_FULL;
  } else if (font_render_hinting_string == "medium") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_MEDIUM;
  } else if (font_render_hinting_string == "slight") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_SLIGHT;
  } else if (font_render_hinting_string == "none") {
    *font_render_hinting = gfx::FontRenderParams::Hinting::HINTING_NONE;
  } else {
    return false;
  }
  return true;
}

base::Value::Dict GetColorDictFromHexColor(const std::string& color_hex) {
  uint32_t color;
  CHECK(base::HexStringToUInt(color_hex, &color))
      << "Expected a hex value for --default-background-color=";

  base::Value::Dict dict;
  dict.Set("r", static_cast<int>((color & 0xff000000) >> 24));
  dict.Set("g", static_cast<int>((color & 0x00ff0000) >> 16));
  dict.Set("b", static_cast<int>((color & 0x0000ff00) >> 8));
  dict.Set("a", static_cast<int>((color & 0x000000ff)));

  return dict;
}

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

std::vector<GURL> ConvertArgumentsToURLs(
    const base::CommandLine::StringVector& args) {
  std::vector<GURL> urls;
  urls.reserve(args.size());
  for (const auto& arg : base::Reversed(args))
    urls.push_back(ConvertArgumentToURL(arg));
  return urls;
}

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

  // TODO(skyostil): Implement custom message pumps.
  DCHECK(!options.message_pump);

  auto browser = std::make_unique<HeadlessBrowserImpl>(
      std::move(on_browser_start_callback), std::move(options));
  HeadlessContentMainDelegate delegate(std::move(browser));
  params.delegate = &delegate;
  return content::ContentMain(std::move(params));
}

bool ValidateCommandLine(const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(switches::kRemoteDebuggingPort) &&
      !command_line.HasSwitch(switches::kRemoteDebuggingPipe)) {
    if (command_line.GetArgs().size() <= 1)
      return true;
    LOG(ERROR) << "Open multiple tabs is only supported when "
               << "remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kDefaultBackgroundColor)) {
    LOG(ERROR) << "Setting default background color is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kDumpDom)) {
    LOG(ERROR) << "Dump DOM is disabled when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kPrintToPDF)) {
    LOG(ERROR) << "Print to PDF is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kRepl)) {
    LOG(ERROR) << "Evaluate Javascript is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kScreenshot)) {
    LOG(ERROR) << "Capture screenshot is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kTimeout)) {
    LOG(ERROR) << "Navigation timeout is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  if (command_line.HasSwitch(switches::kVirtualTimeBudget)) {
    LOG(ERROR) << "Virtual time budget is disabled "
               << "when remote debugging is enabled.";
    return false;
  }
  return true;
}

bool DoWriteFile(const base::FilePath& file_path, std::string file_data) {
  auto file_span = base::make_span(
      reinterpret_cast<const uint8_t*>(file_data.data()), file_data.size());
  bool success = base::WriteFile(file_path, file_span);
  PLOG_IF(ERROR, !success) << "Failed to write file " << file_path;
  if (!success)
    return false;

  LOG(INFO) << file_data.size() << " bytes written to file " << file_path;
  return true;
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

  file_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

  HeadlessBrowserContext::Builder context_builder =
      browser_->CreateBrowserContextBuilder();

  // Retrieve the locale set by InitApplicationLocale() in
  // headless_content_main_delegate.cc in a way that is free of side-effects.
  context_builder.SetAcceptLanguage(base::i18n::GetConfiguredLocale());

  browser_context_ = context_builder.Build();
  browser_->SetDefaultBrowserContext(browser_context_);

  base::CommandLine::StringVector args =
      base::CommandLine::ForCurrentProcess()->GetArgs();

  // If no explicit URL is present, navigate to about:blank, unless we're being
  // driven by debugger.
  if (args.empty() && !base::CommandLine::ForCurrentProcess()->HasSwitch(
                          switches::kRemoteDebuggingPipe)) {
#if BUILDFLAG(IS_WIN)
    args.push_back(L"about:blank");
#else
    args.push_back("about:blank");
#endif
  }

  if (!args.empty()) {
    file_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&ConvertArgumentsToURLs, args),
        base::BindOnce(&HeadlessShell::OnGotURLs, weak_factory_.GetWeakPtr()));
  }
}

void HeadlessShell::OnGotURLs(const std::vector<GURL>& urls) {
  HeadlessWebContents::Builder builder(
      browser_context_->CreateWebContentsBuilder());
  for (const auto& url : urls) {
    HeadlessWebContents* web_contents = builder.SetInitialURL(url).Build();
    if (!web_contents) {
      LOG(ERROR) << "Navigation to " << url << " failed";
      browser_->Shutdown();
      return;
    }
    if (!web_contents_ && !RemoteDebuggingEnabled()) {
      // TODO(jzfeng): Support observing multiple targets.
      url_ = url;
      web_contents_ = web_contents;
      web_contents_->AddObserver(this);
    }
  }
}

void HeadlessShell::Detach() {
  if (!RemoteDebuggingEnabled())
    devtools_client_.DetachClient();

  web_contents_->RemoveObserver(this);
  web_contents_ = nullptr;
}

void HeadlessShell::ShutdownSoon() {
  if (shutdown_pending_)
    return;
  shutdown_pending_ = true;

  DCHECK(browser_);
  browser_->BrowserMainThread()->PostTask(
      FROM_HERE,
      base::BindOnce(&HeadlessShell::Shutdown, weak_factory_.GetWeakPtr()));
}

void HeadlessShell::Shutdown() {
  if (web_contents_)
    web_contents_->Close();
  DCHECK(!web_contents_);

  browser_->Shutdown();
}

void HeadlessShell::DevToolsTargetReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  devtools_client_.AttachToWebContents(
      HeadlessWebContentsImpl::From(web_contents_)->web_contents());
  HeadlessDevToolsTarget* target = web_contents_->GetDevToolsTarget();
  if (!target->IsAttached()) {
    LOG(ERROR) << "Could not attach DevTools target.";
    ShutdownSoon();
    return;
  }

  devtools_client_.AddEventHandler(
      "Inspector.targetCrashed",
      base::BindRepeating(&HeadlessShell::OnTargetCrashed,
                          weak_factory_.GetWeakPtr()));

  devtools_client_.AddEventHandler(
      "Page.loadEventFired",
      base::BindRepeating(&HeadlessShell::OnLoadEventFired,
                          weak_factory_.GetWeakPtr()));
  devtools_client_.SendCommand("Page.enable");

  devtools_client_.AddEventHandler(
      "Emulation.virtualTimeBudgetExpired",
      base::BindRepeating(&HeadlessShell::OnVirtualTimeBudgetExpired,
                          weak_factory_.GetWeakPtr()));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDefaultBackgroundColor)) {
    std::string color_hex =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kDefaultBackgroundColor);
    base::Value::Dict params;
    params.Set("color", GetColorDictFromHexColor(color_hex));
    devtools_client_.SendCommand("Emulation.setDefaultBackgroundColorOverride",
                                 std::move(params));
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVirtualTimeBudget)) {
    std::string budget_ms_ascii =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kVirtualTimeBudget);
    int budget_ms;
    CHECK(base::StringToInt(budget_ms_ascii, &budget_ms))
        << "Expected an integer value for --virtual-time-budget=";

    base::Value::Dict params;
    params.Set("budget", budget_ms);
    params.Set("policy", "pauseIfNetworkFetchesPending");
    devtools_client_.SendCommand("Emulation.setVirtualTimePolicy",
                                 std::move(params));
  } else {
    // Check if the document had already finished loading by the time we
    // attached.
    PollReadyState();
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTimeout)) {
    std::string timeout_ms_ascii =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kTimeout);
    int timeout_ms;
    CHECK(base::StringToInt(timeout_ms_ascii, &timeout_ms))
        << "Expected an integer value for --timeout=";
    browser_->BrowserMainThread()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&HeadlessShell::FetchTimeout,
                       weak_factory_.GetWeakPtr()),
        base::Milliseconds(timeout_ms));
  }
}

void HeadlessShell::HeadlessWebContentsDestroyed() {
  // Detach now, but defer shutdown till the HeadlessWebContents
  // removal is complete.
  Detach();
  ShutdownSoon();
}

void HeadlessShell::FetchTimeout() {
  LOG(INFO) << "Timeout.";
  devtools_client_.SendCommand("Page.stopLoading");
  // After calling page.stopLoading() the page will not fire any
  // life cycle events, so we have to proceed on our own.
  browser_->BrowserMainThread()->PostTask(
      FROM_HERE,
      base::BindOnce(&HeadlessShell::OnPageReady, weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnTargetCrashed(const base::Value::Dict&) {
  LOG(ERROR) << "Abnormal renderer termination.";
  // NB this never gets called if remote debugging is enabled.
  ShutdownSoon();
}

void HeadlessShell::PollReadyState() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We need to check the current location in addition to the ready state to
  // be sure the expected page is ready.
  base::Value::Dict params;
  params.Set("expression",
             "document.readyState + ' ' + document.location.href");
  devtools_client_.SendCommand(
      "Runtime.evaluate", std::move(params),
      base::BindOnce(&HeadlessShell::OnEvaluateReadyStateResult,
                     weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnEvaluateReadyStateResult(base::Value::Dict result) {
  const std::string* result_value =
      result.FindStringByDottedPath("result.result.value");
  if (!result_value)
    return;

  std::stringstream stream(*result_value);
  std::string ready_state;
  std::string url;
  stream >> ready_state;
  stream >> url;

  if (ready_state == "complete" &&
      (url_.spec() == url || url != "about:blank")) {
    OnPageReady();
    return;
  }
}

void HeadlessShell::OnVirtualTimeBudgetExpired(const base::Value::Dict&) {
  OnPageReady();
}

void HeadlessShell::OnLoadEventFired(const base::Value::Dict&) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kVirtualTimeBudget)) {
    return;
  }
  OnPageReady();
}

void HeadlessShell::OnPageReady() {
  if (processed_page_ready_)
    return;
  processed_page_ready_ = true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpDom)) {
    FetchDom();
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kRepl)) {
    LOG(INFO)
        << "Type a Javascript expression to evaluate or \"quit\" to exit.";
    InputExpression();
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kScreenshot)) {
    CaptureScreenshot();
  } else if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                 switches::kPrintToPDF)) {
    PrintToPDF();
  } else {
    ShutdownSoon();
  }
}

void HeadlessShell::FetchDom() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Value::Dict params;
  params.Set(
      "expression",
      "(document.doctype ? new "
      "XMLSerializer().serializeToString(document.doctype) + '\\n' : '') + "
      "document.documentElement.outerHTML");
  devtools_client_.SendCommand(
      "Runtime.evaluate", std::move(params),
      base::BindOnce(&HeadlessShell::OnEvaluateFetchDomResult,
                     weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnEvaluateFetchDomResult(base::Value::Dict result) {
  if (const base::Value::Dict* result_exception_details =
          result.FindDictByDottedPath("result.exceptionDetails")) {
    LOG(ERROR) << "Failed to serialize document:\n"
               << *result_exception_details->FindStringByDottedPath(
                      "exception.description");
  } else if (const std::string* result_value =
                 result.FindStringByDottedPath("result.result.value")) {
    printf("%s\n", result_value->c_str());
  }

  ShutdownSoon();
}

void HeadlessShell::InputExpression() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Note that a real system should read user input asynchronously, because
  // otherwise all other browser activity is suspended (e.g., page loading).
  printf(">>> ");
  std::stringstream expression;
  while (true) {
    int c = fgetc(stdin);
    if (c == '\n')
      break;
    if (c == EOF) {
      // If there's no expression, then quit.
      if (expression.str().size() == 0) {
        printf("\n");
        ShutdownSoon();
        return;
      }
      break;
    }
    expression << static_cast<char>(c);
  }
  if (expression.str() == "quit") {
    ShutdownSoon();
    return;
  }

  base::Value::Dict params;
  params.Set("expression", expression.str());
  devtools_client_.SendCommand(
      "Runtime.evaluate", std::move(params),
      base::BindOnce(&HeadlessShell::OnEvaluateExpressionResult,
                     weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnEvaluateExpressionResult(base::Value::Dict result) {
  std::string result_json;
  base::JSONWriter::Write(result, &result_json);
  printf("%s\n", result_json.c_str());

  InputExpression();
}

void HeadlessShell::CaptureScreenshot() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  devtools_client_.SendCommand(
      "Page.captureScreenshot",
      base::BindOnce(&HeadlessShell::OnCaptureScreenshotResult,
                     weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnCaptureScreenshotResult(base::Value::Dict result) {
  const std::string* result_data = result.FindStringByDottedPath("result.data");
  if (!result_data) {
    LOG(ERROR) << "Capture screenshot failed";
    ShutdownSoon();
    return;
  }

  std::string data;
  if (!base::Base64Decode(*result_data, &data)) {
    LOG(ERROR) << "Invalid screenshot data";
    return;
  }

  WriteFile(switches::kScreenshot, kDefaultScreenshotFileName, std::move(data));
}

void HeadlessShell::PrintToPDF() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::Value::Dict params;
  params.Set("printBackground", true);
  params.Set("preferCSSPageSize", true);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPrintToPDFNoHeader)) {
    params.Set("displayHeaderFooter", false);
  }
  devtools_client_.SendCommand("Page.printToPDF", std::move(params),
                               base::BindOnce(&HeadlessShell::OnPrintToPDFDone,
                                              weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnPrintToPDFDone(base::Value::Dict result) {
  const std::string* result_data = result.FindStringByDottedPath("result.data");
  if (!result_data) {
    LOG(ERROR) << "Print to PDF failed";
    ShutdownSoon();
    return;
  }

  std::string data;
  if (!base::Base64Decode(*result_data, &data)) {
    LOG(ERROR) << "Invalid PDF data";
    return;
  }

  WriteFile(switches::kPrintToPDF, kDefaultPDFFileName, std::move(data));
}

void HeadlessShell::WriteFile(const std::string& file_path_switch,
                              const std::string& default_file_name,
                              std::string data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::FilePath file_name =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          file_path_switch);
  if (file_name.empty())
    file_name = base::FilePath().AppendASCII(default_file_name);

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoWriteFile, file_name, std::move(data)),
      base::BindOnce(&HeadlessShell::OnWriteFileDone,
                     weak_factory_.GetWeakPtr()));
}

void HeadlessShell::OnWriteFileDone(bool success) {
  ShutdownSoon();
}

bool HeadlessShell::RemoteDebuggingEnabled() const {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return (command_line.HasSwitch(switches::kRemoteDebuggingPort) ||
          command_line.HasSwitch(switches::kRemoteDebuggingPipe));
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
  HeadlessShell shell;

#if BUILDFLAG(IS_FUCHSIA)
  // TODO(fuchsia): Remove this when GPU accelerated compositing is ready.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(::switches::kDisableGpu);
#endif

  base::CommandLine& command_line(*base::CommandLine::ForCurrentProcess());
  if (!ValidateCommandLine(command_line))
    return EXIT_FAILURE;

#if BUILDFLAG(IS_MAC)
  command_line.AppendSwitch(os_crypt::switches::kUseMockKeychain);
#endif

  if (command_line.HasSwitch(switches::kDeterministicMode)) {
    command_line.AppendSwitch(switches::kEnableBeginFrameControl);

    // Compositor flags
    command_line.AppendSwitch(::switches::kRunAllCompositorStagesBeforeDraw);
    command_line.AppendSwitch(::switches::kDisableNewContentRenderingTimeout);
    // Ensure that image animations don't resync their animation timestamps when
    // looping back around.
    command_line.AppendSwitch(blink::switches::kDisableImageAnimationResync);

    // Renderer flags
    command_line.AppendSwitch(cc::switches::kDisableThreadedAnimation);
    command_line.AppendSwitch(blink::switches::kDisableThreadedScrolling);
    command_line.AppendSwitch(cc::switches::kDisableCheckerImaging);
  }

  if (command_line.HasSwitch(switches::kEnableBeginFrameControl))
    builder.SetEnableBeginFrameControl(true);

  if (command_line.HasSwitch(switches::kEnableCrashReporter))
    builder.SetCrashReporterEnabled(true);
  if (command_line.HasSwitch(switches::kDisableCrashReporter))
    builder.SetCrashReporterEnabled(false);
  if (command_line.HasSwitch(switches::kCrashDumpsDir)) {
    builder.SetCrashDumpsDir(
        command_line.GetSwitchValuePath(switches::kCrashDumpsDir));
  }

  // Enable devtools if requested, by specifying a port (and optional address).
  if (command_line.HasSwitch(::switches::kRemoteDebuggingPort)) {
    std::string address = kUseLocalHostForDevToolsHttpServer;
    if (command_line.HasSwitch(switches::kRemoteDebuggingAddress)) {
      address =
          command_line.GetSwitchValueASCII(switches::kRemoteDebuggingAddress);
      net::IPAddress parsed_address;
      if (!parsed_address.AssignFromIPLiteral(address)) {
        LOG(ERROR) << "Invalid devtools server address";
        return EXIT_FAILURE;
      }
    }
    int parsed_port;
    std::string port_str =
        command_line.GetSwitchValueASCII(::switches::kRemoteDebuggingPort);
    if (!base::StringToInt(port_str, &parsed_port) ||
        !base::IsValueInRangeForNumericType<uint16_t>(parsed_port)) {
      LOG(ERROR) << "Invalid devtools server port";
      return EXIT_FAILURE;
    }
    const net::HostPortPair endpoint(address,
                                     base::checked_cast<uint16_t>(parsed_port));
    builder.EnableDevToolsServer(endpoint);
  }
  if (command_line.HasSwitch(::switches::kRemoteDebuggingPipe))
    builder.EnableDevToolsPipe();

  if (command_line.HasSwitch(switches::kProxyServer)) {
    std::string proxy_server =
        command_line.GetSwitchValueASCII(switches::kProxyServer);
    auto proxy_config = std::make_unique<net::ProxyConfig>();
    proxy_config->proxy_rules().ParseFromString(proxy_server);
    if (command_line.HasSwitch(switches::kProxyBypassList)) {
      std::string bypass_list =
          command_line.GetSwitchValueASCII(switches::kProxyBypassList);
      proxy_config->proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }
    builder.SetProxyConfig(std::move(proxy_config));
  }

  if (command_line.HasSwitch(switches::kUseGL)) {
    builder.SetGLImplementation(
        command_line.GetSwitchValueASCII(switches::kUseGL));
  }

  if (command_line.HasSwitch(switches::kUseANGLE)) {
    builder.SetANGLEImplementation(
        command_line.GetSwitchValueASCII(switches::kUseANGLE));
  }

  if (command_line.HasSwitch(switches::kUserDataDir)) {
    builder.SetUserDataDir(
        command_line.GetSwitchValuePath(switches::kUserDataDir));
    if (!command_line.HasSwitch(switches::kIncognito))
      builder.SetIncognitoMode(false);
  }

  if (command_line.HasSwitch(switches::kWindowSize)) {
    std::string window_size =
        command_line.GetSwitchValueASCII(switches::kWindowSize);
    gfx::Size parsed_window_size;
    if (!ParseWindowSize(window_size, &parsed_window_size)) {
      LOG(ERROR) << "Malformed window size";
      return EXIT_FAILURE;
    }
    builder.SetWindowSize(parsed_window_size);
  }

  if (command_line.HasSwitch(switches::kHideScrollbars)) {
    builder.SetOverrideWebPreferencesCallback(
        base::BindRepeating([](blink::web_pref::WebPreferences* preferences) {
          preferences->hide_scrollbars = true;
        }));
  }

  if (command_line.HasSwitch(switches::kUserAgent)) {
    std::string ua = command_line.GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua))
      builder.SetUserAgent(ua);
  }

  if (command_line.HasSwitch(switches::kFontRenderHinting)) {
    std::string font_render_hinting_string =
        command_line.GetSwitchValueASCII(switches::kFontRenderHinting);
    gfx::FontRenderParams::Hinting font_render_hinting;
    if (ParseFontRenderHinting(font_render_hinting_string,
                               &font_render_hinting)) {
      builder.SetFontRenderHinting(font_render_hinting);
    } else {
      LOG(ERROR) << "Unknown font-render-hinting parameter value";
      return EXIT_FAILURE;
    }
  }

  if (command_line.HasSwitch(switches::kBlockNewWebContents))
    builder.SetBlockNewWebContents(true);

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

  if (command_line.HasSwitch(switches::kUserAgent)) {
    std::string ua = command_line.GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua))
      builder.SetUserAgent(ua);
  }

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
