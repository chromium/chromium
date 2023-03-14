// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_content_browser_client.h"

#include <string>
#include <unordered_set>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "components/embedder_support/switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_browser_main_parts.h"
#include "headless/lib/browser/headless_devtools_manager_delegate.h"
#include "headless/public/switches.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/port_util.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_private_key.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/switches.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/switches.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/app/crash_switches.h"  // nogncheck
#include "components/crash/core/app/crashpad.h"        // nogncheck
#include "content/public/common/content_descriptors.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if defined(HEADLESS_USE_POLICY)
#include "components/policy/content/policy_blocklist_navigation_throttle.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#endif  // defined(HEADLESS_USE_POLICY)

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/browser/headless/headless_print_manager.h"
#endif  // defined(ENABLE_PRINTING)

namespace headless {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
namespace {

int GetCrashSignalFD(const base::CommandLine& command_line,
                     const HeadlessBrowser::Options& options) {
  int fd;
  pid_t pid;
  return crash_reporter::GetHandlerSocket(&fd, &pid) ? fd : -1;
}

}  // namespace
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Implements a stub BadgeService. This implementation does nothing, but is
// required because inbound Mojo messages which do not have a registered
// handler are considered an error, and the render process is terminated.
// See https://crbug.com/1090429
class HeadlessContentBrowserClient::StubBadgeService
    : public blink::mojom::BadgeService {
 public:
  StubBadgeService() = default;
  StubBadgeService(const StubBadgeService&) = delete;
  StubBadgeService& operator=(const StubBadgeService&) = delete;
  ~StubBadgeService() override = default;

  void Bind(mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  void Reset() {}

  // blink::mojom::BadgeService:
  void SetBadge(blink::mojom::BadgeValuePtr value) override {}
  void ClearBadge() override {}

 private:
  mojo::ReceiverSet<blink::mojom::BadgeService> receivers_;
};

HeadlessContentBrowserClient::HeadlessContentBrowserClient(
    HeadlessBrowserImpl* browser)
    : browser_(browser) {}

HeadlessContentBrowserClient::~HeadlessContentBrowserClient() = default;

std::unique_ptr<content::BrowserMainParts>
HeadlessContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  auto browser_main_parts =
      std::make_unique<HeadlessBrowserMainParts>(browser_);

  browser_->set_browser_main_parts(browser_main_parts.get());

  return browser_main_parts;
}

void HeadlessContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
  prefs->lazy_load_enabled = browser_->options()->lazy_load_enabled;
}

void HeadlessContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<blink::mojom::BadgeService>(base::BindRepeating(
      &HeadlessContentBrowserClient::BindBadgeService, base::Unretained(this)));
}

void HeadlessContentBrowserClient::
    RegisterAssociatedInterfaceBindersForRenderFrameHost(
        content::RenderFrameHost& render_frame_host,
        blink::AssociatedInterfaceRegistry& associated_registry) {
  // TODO(https://crbug.com/1265864): Move the registry logic below to a
  // dedicated file to ensure security review coverage.
#if BUILDFLAG(ENABLE_PRINTING)
  associated_registry.AddInterface<printing::mojom::PrintManagerHost>(
      base::BindRepeating(
          [](content::RenderFrameHost* render_frame_host,
             mojo::PendingAssociatedReceiver<printing::mojom::PrintManagerHost>
                 receiver) {
            HeadlessPrintManager::BindPrintManagerHost(std::move(receiver),
                                                       render_frame_host);
          },
          &render_frame_host));
#endif
}

std::unique_ptr<content::DevToolsManagerDelegate>
HeadlessContentBrowserClient::CreateDevToolsManagerDelegate() {
  return std::make_unique<HeadlessDevToolsManagerDelegate>(
      browser_->GetWeakPtr());
}

content::GeneratedCodeCacheSettings
HeadlessContentBrowserClient::GetGeneratedCodeCacheSettings(
    content::BrowserContext* context) {
  // If we pass 0 for size, disk_cache will pick a default size using the
  // heuristics based on available disk size. These are implemented in
  // disk_cache::PreferredCacheSize in net/disk_cache/cache_util.cc.
  return content::GeneratedCodeCacheSettings(true, 0, context->GetPath());
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void HeadlessContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const base::CommandLine& command_line,
    int child_process_id,
    content::PosixFileDescriptorInfo* mappings) {
  int crash_signal_fd = GetCrashSignalFD(command_line, *browser_->options());
  if (crash_signal_fd >= 0)
    mappings->Share(kCrashDumpSignal, crash_signal_fd);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

void HeadlessContentBrowserClient::AppendExtraCommandLineSwitches(
    base::CommandLine* command_line,
    int child_process_id) {
  // NOTE: We may be called on the UI or IO thread. If called on the IO thread,
  // |browser_| may have already been destroyed.

  command_line->AppendSwitch(::switches::kHeadless);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  int fd;
  pid_t pid;
  if (crash_reporter::GetHandlerSocket(&fd, &pid)) {
    command_line->AppendSwitchASCII(
        crash_reporter::switches::kCrashpadHandlerPid,
        base::NumberToString(pid));
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  const base::CommandLine& old_command_line(
      *base::CommandLine::ForCurrentProcess());
  if (old_command_line.HasSwitch(switches::kDisablePDFTagging))
    command_line->AppendSwitch(switches::kDisablePDFTagging);

  // If we're spawning a renderer, then override the language switch.
  std::string process_type =
      command_line->GetSwitchValueASCII(::switches::kProcessType);
  if (process_type == ::switches::kRendererProcess) {
    // Renderer processes are initialized on the UI thread, so this is safe.
    content::RenderProcessHost* render_process_host =
        content::RenderProcessHost::FromID(child_process_id);
    if (render_process_host) {
      HeadlessBrowserContextImpl* headless_browser_context_impl =
          HeadlessBrowserContextImpl::From(
              render_process_host->GetBrowserContext());

      std::vector<base::StringPiece> languages = base::SplitStringPiece(
          headless_browser_context_impl->options()->accept_language(), ",",
          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (!languages.empty()) {
        command_line->AppendSwitchASCII(::switches::kLang,
                                        std::string(languages[0]));
      }
    }

    // Please keep this in alphabetical order.
    static const char* const kSwitchNames[] = {
        embedder_support::kOriginTrialDisabledFeatures,
        embedder_support::kOriginTrialDisabledTokens,
        embedder_support::kOriginTrialPublicKey,
    };
    command_line->CopySwitchesFrom(old_command_line, kSwitchNames,
                                   std::size(kSwitchNames));
  }
}

std::string HeadlessContentBrowserClient::GetApplicationLocale() {
  return base::i18n::GetConfiguredLocale();
}

std::string HeadlessContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return browser_->options()->accept_language;
}

void HeadlessContentBrowserClient::AllowCertificateError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool is_primary_main_frame_request,
    bool strict_enforcement,
    base::OnceCallback<void(content::CertificateRequestResultType)> callback) {
  if (!callback.is_null()) {
    // If --allow-insecure-localhost is specified, and the request
    // was for localhost, then the error was not fatal.
    bool allow_localhost = base::CommandLine::ForCurrentProcess()->HasSwitch(
        ::switches::kAllowInsecureLocalhost);
    if (allow_localhost && net::IsLocalhost(request_url)) {
      std::move(callback).Run(
          content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE);
      return;
    }

    std::move(callback).Run(content::CERTIFICATE_REQUEST_RESULT_TYPE_DENY);
  }
}

base::OnceClosure HeadlessContentBrowserClient::SelectClientCertificate(
    content::WebContents* web_contents,
    net::SSLCertRequestInfo* cert_request_info,
    net::ClientCertIdentityList client_certs,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  delegate->ContinueWithCertificate(nullptr, nullptr);
  return base::OnceClosure();
}

bool HeadlessContentBrowserClient::ShouldEnableStrictSiteIsolation() {
  // TODO(lukasza): https://crbug.com/869494: Instead of overriding
  // ShouldEnableStrictSiteIsolation, //headless should inherit the default
  // site-per-process setting from //content - this way tools (tests, but also
  // production cases like screenshot or pdf generation) based on //headless
  // will use a mode that is actually shipping in Chrome.
  return false;
}

bool HeadlessContentBrowserClient::IsSharedStorageAllowed(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* rfh,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin) {
  return true;
}

bool HeadlessContentBrowserClient::IsSharedStorageSelectURLAllowed(
    content::BrowserContext* browser_context,
    const url::Origin& top_frame_origin,
    const url::Origin& accessing_origin) {
  return true;
}

void HeadlessContentBrowserClient::ConfigureNetworkContextParams(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    ::network::mojom::NetworkContextParams* network_context_params,
    ::cert_verifier::mojom::CertVerifierCreationParams*
        cert_verifier_creation_params) {
  HeadlessBrowserContextImpl::From(context)->ConfigureNetworkContextParams(
      in_memory, relative_partition_path, network_context_params,
      cert_verifier_creation_params);
}

std::string HeadlessContentBrowserClient::GetProduct() {
  return HeadlessBrowser::GetProductNameAndVersion();
}

std::string HeadlessContentBrowserClient::GetUserAgent() {
  return browser_->options()->user_agent;
}

blink::UserAgentMetadata HeadlessContentBrowserClient::GetUserAgentMetadata() {
  return HeadlessBrowser::GetUserAgentMetadata();
}

void HeadlessContentBrowserClient::BindBadgeService(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::BadgeService> receiver) {
  if (!stub_badge_service_)
    stub_badge_service_ = std::make_unique<StubBadgeService>();

  stub_badge_service_->Bind(std::move(receiver));
}

bool HeadlessContentBrowserClient::CanAcceptUntrustedExchangesIfNeeded() {
  // We require --user-data-dir flag too so that no dangerous changes are made
  // in the user's regular profile.
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUserDataDir);
}

device::GeolocationManager*
HeadlessContentBrowserClient::GetGeolocationManager() {
#if BUILDFLAG(IS_MAC)
  return browser_->browser_main_parts()->GetGeolocationManager();
#else
  return nullptr;
#endif
}

#if defined(HEADLESS_USE_POLICY)
std::vector<std::unique_ptr<content::NavigationThrottle>>
HeadlessContentBrowserClient::CreateThrottlesForNavigation(
    content::NavigationHandle* handle) {
  std::vector<std::unique_ptr<content::NavigationThrottle>> throttles;

  // Avoid creating naviagtion throttle if preferences are not available
  // (happens in tests).
  if (browser_->GetPrefs()) {
    throttles.push_back(std::make_unique<PolicyBlocklistNavigationThrottle>(
        handle, handle->GetWebContents()->GetBrowserContext()));
  }

  return throttles;
}
#endif  // defined(HEADLESS_USE_POLICY)

void HeadlessContentBrowserClient::OnNetworkServiceCreated(
    ::network::mojom::NetworkService* network_service) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kExplicitlyAllowedPorts))
    return;

  std::string comma_separated_ports =
      command_line->GetSwitchValueASCII(switches::kExplicitlyAllowedPorts);
  const auto port_list = base::SplitStringPiece(
      comma_separated_ports, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<uint16_t> explicitly_allowed_ports;
  for (const auto port_str : port_list) {
    int port;
    if (!base::StringToInt(port_str, &port))
      continue;
    if (!net::IsPortValid(port))
      continue;
    explicitly_allowed_ports.push_back(port);
  }

  network_service->SetExplicitlyAllowedPorts(explicitly_allowed_ports);
}

}  // namespace headless
