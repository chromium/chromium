// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extensions_browser_client.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/core_extensions_browser_api_provider.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/mojo/interface_registration.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/updater/null_extension_cache.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/shell/browser/api/runtime/shell_runtime_api_delegate.h"
#include "extensions/shell/browser/delegates/shell_kiosk_delegate.h"
#include "extensions/shell/browser/shell_extension_host_delegate.h"
#include "extensions/shell/browser/shell_extension_system_factory.h"
#include "extensions/shell/browser/shell_extension_web_contents_observer.h"
#include "extensions/shell/browser/shell_extensions_api_client.h"
#include "extensions/shell/browser/shell_extensions_browser_api_provider.h"
#include "extensions/shell/browser/shell_navigation_ui_data.h"

#if defined(OS_CHROMEOS)
#include "chromeos/login/login_state.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

ShellExtensionsBrowserClient::ShellExtensionsBrowserClient()
    : api_client_(new ShellExtensionsAPIClient),
      extension_cache_(new NullExtensionCache()) {
  // app_shell does not have a concept of channel yet, so leave UNKNOWN to
  // enable all channel-dependent extension APIs.
  SetCurrentChannel(version_info::Channel::UNKNOWN);

  AddAPIProvider(std::make_unique<CoreExtensionsBrowserAPIProvider>());
  AddAPIProvider(std::make_unique<ShellExtensionsBrowserAPIProvider>());
}

ShellExtensionsBrowserClient::~ShellExtensionsBrowserClient() {
}

bool ShellExtensionsBrowserClient::IsShuttingDown() {
  return false;
}

bool ShellExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool ShellExtensionsBrowserClient::IsValidContext(BrowserContext* context) {
  DCHECK(browser_context_);
  return context == browser_context_;
}

bool ShellExtensionsBrowserClient::IsSameContext(BrowserContext* first,
                                                 BrowserContext* second) {
  return first == second;
}

bool ShellExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return false;
}

BrowserContext* ShellExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  // app_shell only supports a single context.
  return NULL;
}

BrowserContext* ShellExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return context;
}

#if defined(OS_CHROMEOS)
std::string ShellExtensionsBrowserClient::GetUserIdHashFromContext(
    content::BrowserContext* context) {
  if (!chromeos::LoginState::IsInitialized())
    return "";
  return chromeos::LoginState::Get()->primary_user_hash();
}
#endif

bool ShellExtensionsBrowserClient::IsGuestSession(
    BrowserContext* context) const {
  return false;
}

bool ShellExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return false;
}

bool ShellExtensionsBrowserClient::CanExtensionCrossIncognito(
    const Extension* extension,
    content::BrowserContext* context) const {
  return false;
}

net::URLRequestJob*
ShellExtensionsBrowserClient::MaybeCreateResourceBundleRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
  return NULL;
}

base::FilePath ShellExtensionsBrowserClient::GetBundleResourcePath(
    const network::ResourceRequest& request,
    const base::FilePath& extension_resources_path,
    int* resource_id) const {
  *resource_id = 0;
  return base::FilePath();
}

void ShellExtensionsBrowserClient::LoadResourceFromResourceBundle(
    const network::ResourceRequest& request,
    network::mojom::URLLoaderRequest loader,
    const base::FilePath& resource_relative_path,
    int resource_id,
    const std::string& content_security_policy,
    network::mojom::URLLoaderClientPtr client,
    bool send_cors_header) {
  NOTREACHED() << "Load resources from bundles not supported.";
}

bool ShellExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    const GURL& url,
    content::ResourceType resource_type,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map) {
  bool allowed = false;
  if (url_request_util::AllowCrossRendererResourceLoad(
          url, resource_type, page_transition, child_id, is_incognito,
          extension, extensions, process_map, &allowed)) {
    return allowed;
  }

  // Couldn't determine if resource is allowed. Block the load.
  return false;
}

PrefService* ShellExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  DCHECK(pref_service_);
  return pref_service_;
}

void ShellExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<ExtensionPrefsObserver*>* observers) const {
}

ProcessManagerDelegate*
ShellExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return NULL;
}

std::unique_ptr<ExtensionHostDelegate>
ShellExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return base::WrapUnique(new ShellExtensionHostDelegate);
}

bool ShellExtensionsBrowserClient::DidVersionUpdate(BrowserContext* context) {
  // TODO(jamescook): We might want to tell extensions when app_shell updates.
  return false;
}

void ShellExtensionsBrowserClient::PermitExternalProtocolHandler() {
}

bool ShellExtensionsBrowserClient::IsInDemoMode() {
  return false;
}

bool ShellExtensionsBrowserClient::IsScreensaverInDemoMode(
    const std::string& app_id) {
  return false;
}

bool ShellExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

bool ShellExtensionsBrowserClient::IsAppModeForcedForApp(
    const ExtensionId& extension_id) {
  return false;
}

bool ShellExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
  return false;
}

ExtensionSystemProvider*
ShellExtensionsBrowserClient::GetExtensionSystemFactory() {
  return ShellExtensionSystemFactory::GetInstance();
}

void ShellExtensionsBrowserClient::RegisterExtensionInterfaces(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) const {
  RegisterInterfacesForExtension(registry, render_frame_host, extension);
}

std::unique_ptr<RuntimeAPIDelegate>
ShellExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return std::make_unique<ShellRuntimeAPIDelegate>(context);
}

const ComponentExtensionResourceManager*
ShellExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return NULL;
}

void ShellExtensionsBrowserClient::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::Bind(&ShellExtensionsBrowserClient::BroadcastEventToRenderers,
                   base::Unretained(this), histogram_value, event_name,
                   base::Passed(&args)));
    return;
  }

  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(args)));
  EventRouter::Get(browser_context_)->BroadcastEvent(std::move(event));
}

net::NetLog* ShellExtensionsBrowserClient::GetNetLog() {
  return NULL;
}

ExtensionCache* ShellExtensionsBrowserClient::GetExtensionCache() {
  return extension_cache_.get();
}

bool ShellExtensionsBrowserClient::IsBackgroundUpdateAllowed() {
  return true;
}

bool ShellExtensionsBrowserClient::IsMinBrowserVersionSupported(
    const std::string& min_version) {
  return true;
}

void ShellExtensionsBrowserClient::SetAPIClientForTest(
    ExtensionsAPIClient* api_client) {
  api_client_.reset(api_client);
}

ExtensionWebContentsObserver*
ShellExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return ShellExtensionWebContentsObserver::FromWebContents(web_contents);
}

ExtensionNavigationUIData*
ShellExtensionsBrowserClient::GetExtensionNavigationUIData(
    net::URLRequest* request) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return nullptr;
  ShellNavigationUIData* navigation_data =
      static_cast<ShellNavigationUIData*>(info->GetNavigationUIData());
  if (!navigation_data)
    return nullptr;
  return navigation_data->GetExtensionNavigationUIData();
}

KioskDelegate* ShellExtensionsBrowserClient::GetKioskDelegate() {
  if (!kiosk_delegate_)
    kiosk_delegate_.reset(new ShellKioskDelegate());
  return kiosk_delegate_.get();
}

bool ShellExtensionsBrowserClient::IsLockScreenContext(
    content::BrowserContext* context) {
  return false;
}

std::string ShellExtensionsBrowserClient::GetApplicationLocale() {
  // TODO(michaelpg): Use system locale.
  return "en-US";
}

void ShellExtensionsBrowserClient::InitWithBrowserContext(
    content::BrowserContext* context,
    PrefService* pref_service) {
  DCHECK(!browser_context_);
  DCHECK(!pref_service_);
  browser_context_ = context;
  pref_service_ = pref_service;
}

}  // namespace extensions
