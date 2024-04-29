// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/web_engine_main_delegate.h"

#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/fuchsia/intl_profile_watcher.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "fuchsia_web/common/init_logging.h"
#include "fuchsia_web/webengine/browser/web_engine_browser_main.h"
#include "fuchsia_web/webengine/browser/web_engine_content_browser_client.h"
#include "fuchsia_web/webengine/common/cors_exempt_headers.h"
#include "fuchsia_web/webengine/common/web_engine_content_client.h"
#include "fuchsia_web/webengine/renderer/web_engine_content_renderer_client.h"
#include "fuchsia_web/webengine/switches.h"
#include "google_apis/buildflags.h"
#include "google_apis/google_api_keys.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace {

WebEngineMainDelegate* g_current_web_engine_main_delegate = nullptr;

void InitializeResources() {
  constexpr char kCommonResourcesPakPath[] = "web_engine_common_resources.pak";

  constexpr char kWebUiGeneratedResourcesPakPath[] =
      "ui/resources/webui_resources.pak";

  base::FilePath asset_root;
  bool result = base::PathService::Get(base::DIR_ASSETS, &asset_root);
  DCHECK(result);

  // Initialize the process-global ResourceBundle, and manually load the
  // WebEngine locale-agnostic resources.
  const std::string locale = ui::ResourceBundle::InitSharedInstanceWithLocale(
      base::i18n::GetConfiguredLocale(), nullptr,
      ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  ui::SetSupportedResourceScaleFactors({ui::k100Percent});
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      asset_root.Append(kCommonResourcesPakPath), ui::kScaleFactorNone);
  VLOG(1) << "Loaded resources including locale: " << locale;

  // Conditionally load WebUI resource PAK if visible from namespace.
  const base::FilePath webui_resources_path =
      asset_root.Append(kWebUiGeneratedResourcesPakPath);
  if (base::PathExists(webui_resources_path)) {
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        webui_resources_path, ui::kScaleFactorNone);
  }
}

}  // namespace

// static
WebEngineMainDelegate* WebEngineMainDelegate::GetInstanceForTest() {
  return g_current_web_engine_main_delegate;
}

WebEngineMainDelegate::WebEngineMainDelegate() {
  g_current_web_engine_main_delegate = this;
}

WebEngineMainDelegate::~WebEngineMainDelegate() = default;

std::optional<int> WebEngineMainDelegate::BasicStartupComplete() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!InitLoggingFromCommandLine(*command_line)) {
    return 1;
  }

  SetCorsExemptHeaders(base::SplitString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kCorsExemptHeaders),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  return std::nullopt;
}

void WebEngineMainDelegate::PreSandboxStartup() {
  // Early during startup, configure the process with the primary locale and
  // load resources. If locale-specific resources are loaded then they must be
  // explicitly reloaded after each change to the primary locale.
  // In the browser process the locale determines the accept-language header
  // contents, and is supplied to renderers for Blink to report to web content.
  std::string initial_locale =
      base::FuchsiaIntlProfileWatcher::GetPrimaryLocaleIdForInitialization();
  base::i18n::SetICUDefaultLocale(initial_locale);

  InitializeResources();
}

std::optional<int> WebEngineMainDelegate::PreBrowserMain() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kGoogleApiKey)) {
#if BUILDFLAG(SUPPORT_EXTERNAL_GOOGLE_API_KEY)
    google_apis::SetAPIKey(
        command_line->GetSwitchValueASCII(switches::kGoogleApiKey));
#else
    LOG(WARNING) << "Ignored " << switches::kGoogleApiKey;
#endif
  }

  return std::nullopt;
}

absl::variant<int, content::MainFunctionParams>
WebEngineMainDelegate::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  if (!process_type.empty())
    return std::move(main_function_params);

  return WebEngineBrowserMain(std::move(main_function_params));
}

content::ContentClient* WebEngineMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<WebEngineContentClient>();
  return content_client_.get();
}

content::ContentBrowserClient*
WebEngineMainDelegate::CreateContentBrowserClient() {
  DCHECK(!browser_client_);
  browser_client_ = std::make_unique<WebEngineContentBrowserClient>();
  return browser_client_.get();
}

content::ContentRendererClient*
WebEngineMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<WebEngineContentRendererClient>();
  return renderer_client_.get();
}
