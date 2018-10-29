// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/app/content_main.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/common/content_switches.h"
#include "headless/app/headless_shell_switches.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_devtools_agent_host_client.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/lib/headless_content_main_delegate.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/events/devices/device_data_manager.h"

#if defined(USE_NSS_CERTS)
#include "net/cert_net/nss_ocsp.h"
#endif

namespace headless {
namespace {

int RunContentMain(
    HeadlessBrowser::Options options,
    base::OnceCallback<void(HeadlessBrowser*)> on_browser_start_callback) {
  content::ContentMainParams params(nullptr);
#if defined(OS_WIN)
  // Sandbox info has to be set and initialized.
  CHECK(options.sandbox_info);
  params.instance = options.instance;
  params.sandbox_info = std::move(options.sandbox_info);
#elif !defined(OS_ANDROID)
  params.argc = options.argc;
  params.argv = options.argv;
#endif

  // TODO(skyostil): Implement custom message pumps.
  DCHECK(!options.message_pump);

  std::unique_ptr<HeadlessBrowserImpl> browser(new HeadlessBrowserImpl(
      std::move(on_browser_start_callback), std::move(options)));
  HeadlessContentMainDelegate delegate(std::move(browser));
  params.delegate = &delegate;
  return content::ContentMain(params);
}

}  // namespace

const base::FilePath::CharType kDefaultProfileName[] =
    FILE_PATH_LITERAL("Default");

HeadlessBrowserImpl::HeadlessBrowserImpl(
    base::OnceCallback<void(HeadlessBrowser*)> on_start_callback,
    HeadlessBrowser::Options options)
    : on_start_callback_(std::move(on_start_callback)),
      options_(std::move(options)),
      browser_main_parts_(nullptr),
      default_browser_context_(nullptr),
      agent_host_(nullptr),
      weak_ptr_factory_(this) {}

HeadlessBrowserImpl::~HeadlessBrowserImpl() = default;

HeadlessBrowserContext::Builder
HeadlessBrowserImpl::CreateBrowserContextBuilder() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return HeadlessBrowserContext::Builder(this);
}

scoped_refptr<base::SingleThreadTaskRunner>
HeadlessBrowserImpl::BrowserMainThread() const {
  return base::CreateSingleThreadTaskRunnerWithTraits(
      {content::BrowserThread::UI});
}

void HeadlessBrowserImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  weak_ptr_factory_.InvalidateWeakPtrs();
  browser_contexts_.clear();
  if (system_request_context_manager_) {
    content::BrowserThread::DeleteSoon(
        content::BrowserThread::IO, FROM_HERE,
        system_request_context_manager_.release());
  }
  browser_main_parts_->QuitMainMessageLoop();
}

std::vector<HeadlessBrowserContext*>
HeadlessBrowserImpl::GetAllBrowserContexts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<HeadlessBrowserContext*> result;
  result.reserve(browser_contexts_.size());

  for (const auto& browser_context_pair : browser_contexts_) {
    result.push_back(browser_context_pair.second.get());
  }

  return result;
}

HeadlessBrowserMainParts* HeadlessBrowserImpl::browser_main_parts() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return browser_main_parts_;
}

void HeadlessBrowserImpl::set_browser_main_parts(
    HeadlessBrowserMainParts* browser_main_parts) {
  DCHECK(!browser_main_parts_);
  browser_main_parts_ = browser_main_parts;
}

void HeadlessBrowserImpl::RunOnStartCallback() {
  // We don't support the tethering domain on this agent host.
  agent_host_ = content::DevToolsAgentHost::CreateForBrowser(
      nullptr, content::DevToolsAgentHost::CreateServerSocketCallback());

  PlatformStart();
  std::move(on_start_callback_).Run(this);
}

HeadlessBrowserContext* HeadlessBrowserImpl::CreateBrowserContext(
    HeadlessBrowserContext::Builder* builder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<HeadlessBrowserContextImpl> browser_context =
      HeadlessBrowserContextImpl::Create(builder);

  if (!browser_context) {
    return nullptr;
  }

  HeadlessBrowserContext* result = browser_context.get();

  browser_contexts_[browser_context->Id()] = std::move(browser_context);

  return result;
}

void HeadlessBrowserImpl::DestroyBrowserContext(
    HeadlessBrowserContextImpl* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int erased = browser_contexts_.erase(browser_context->Id());
  DCHECK(erased);
  if (default_browser_context_ == browser_context)
    SetDefaultBrowserContext(nullptr);
}

void HeadlessBrowserImpl::SetDefaultBrowserContext(
    HeadlessBrowserContext* browser_context) {
  DCHECK(!browser_context ||
         this == HeadlessBrowserContextImpl::From(browser_context)->browser());

  default_browser_context_ = browser_context;

  if (default_browser_context_ && !system_request_context_manager_) {
    system_request_context_manager_ =
        HeadlessRequestContextManager::CreateSystemContext(
            HeadlessBrowserContextImpl::From(browser_context)->options());
  }
}

HeadlessBrowserContext* HeadlessBrowserImpl::GetDefaultBrowserContext() {
  return default_browser_context_;
}

base::WeakPtr<HeadlessBrowserImpl> HeadlessBrowserImpl::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return weak_ptr_factory_.GetWeakPtr();
}

HeadlessWebContents* HeadlessBrowserImpl::GetWebContentsForDevToolsAgentHostId(
    const std::string& devtools_agent_host_id) {
  for (HeadlessBrowserContext* context : GetAllBrowserContexts()) {
    HeadlessWebContents* web_contents =
        context->GetWebContentsForDevToolsAgentHostId(devtools_agent_host_id);
    if (web_contents)
      return web_contents;
  }
  return nullptr;
}

HeadlessWebContentsImpl* HeadlessBrowserImpl::GetWebContentsForWindowId(
    const int window_id) {
  for (HeadlessBrowserContext* context : GetAllBrowserContexts()) {
    for (HeadlessWebContents* web_contents : context->GetAllWebContents()) {
      auto* contents = HeadlessWebContentsImpl::From(web_contents);
      if (contents->window_id() == window_id) {
        return contents;
      }
    }
  }
  return nullptr;
}

HeadlessBrowserContext* HeadlessBrowserImpl::GetBrowserContextForId(
    const std::string& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto find_it = browser_contexts_.find(id);
  if (find_it == browser_contexts_.end())
    return nullptr;
  return find_it->second.get();
}

HeadlessDevToolsTarget* HeadlessBrowserImpl::GetDevToolsTarget() {
  return agent_host_ ? this : nullptr;
}

std::unique_ptr<HeadlessDevToolsChannel>
HeadlessBrowserImpl::CreateDevToolsChannel() {
  DCHECK(agent_host_);
  return std::make_unique<HeadlessDevToolsAgentHostClient>(agent_host_);
}

void HeadlessBrowserImpl::AttachClient(HeadlessDevToolsClient* client) {
  client->AttachToChannel(CreateDevToolsChannel());
}

void HeadlessBrowserImpl::DetachClient(HeadlessDevToolsClient* client) {
  client->DetachFromChannel();
}

bool HeadlessBrowserImpl::IsAttached() {
  DCHECK(agent_host_);
  return agent_host_->IsAttached();
}

#if defined(OS_WIN)
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
#endif  // defined(OS_WIN)
  const base::CommandLine& command_line(
      *base::CommandLine::ForCurrentProcess());

  if (!command_line.HasSwitch(::switches::kProcessType))
    return;

  if (command_line.HasSwitch(switches::kUserAgent)) {
    std::string ua = command_line.GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(ua))
      builder.SetUserAgent(ua);
  }

  exit(RunContentMain(builder.Build(),
                      base::OnceCallback<void(HeadlessBrowser*)>()));
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
