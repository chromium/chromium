// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/web_engine_main_delegate.h"

#include <utility>

#include "base/base_paths.h"
#include "base/base_paths_fuchsia.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/base/init_logging.h"
#include "fuchsia/engine/browser/web_engine_browser_main.h"
#include "fuchsia/engine/browser/web_engine_content_browser_client.h"
#include "fuchsia/engine/common/cors_exempt_headers.h"
#include "fuchsia/engine/common/web_engine_content_client.h"
#include "fuchsia/engine/renderer/web_engine_content_renderer_client.h"
#include "fuchsia/engine/switches.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

WebEngineMainDelegate* g_current_web_engine_main_delegate = nullptr;

void InitializeResources() {
  constexpr char kWebEnginePakPath[] = "web_engine.pak";
  constexpr char kWebUiResourcesPakPath[] = "ui/resources/webui_resources.pak";
  constexpr char kWebUiGeneratedResourcesPakPath[] =
      "ui/resources/webui_generated_resources.pak";

  base::FilePath asset_root;
  bool result = base::PathService::Get(base::DIR_ASSETS, &asset_root);
  DCHECK(result);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      asset_root.Append(kWebEnginePakPath));

  // Conditionally load WebUI resource PAK if visible from namespace.
  base::FilePath webui_resources_path =
      asset_root.Append(kWebUiResourcesPakPath);
  if (base::PathExists(webui_resources_path)) {
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        webui_resources_path, ui::SCALE_FACTOR_NONE);
  }

  base::FilePath webui_generated_resources_path =
      asset_root.Append(kWebUiGeneratedResourcesPakPath);
  if (base::PathExists(webui_generated_resources_path)) {
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        webui_generated_resources_path, ui::SCALE_FACTOR_NONE);
  }
}

}  // namespace

// static
WebEngineMainDelegate* WebEngineMainDelegate::GetInstanceForTest() {
  return g_current_web_engine_main_delegate;
}

WebEngineMainDelegate::WebEngineMainDelegate(
    fidl::InterfaceRequest<fuchsia::web::Context> request)
    : request_(std::move(request)) {
  g_current_web_engine_main_delegate = this;
}

WebEngineMainDelegate::~WebEngineMainDelegate() = default;

bool WebEngineMainDelegate::BasicStartupComplete(int* exit_code) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!cr_fuchsia::InitLoggingFromCommandLine(*command_line)) {
    *exit_code = 1;
    return true;
  }

  SetCorsExemptHeaders(base::SplitString(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueNative(
          switches::kCorsExemptHeaders),
      ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY));

  return false;
}

void WebEngineMainDelegate::PreSandboxStartup() {
  InitializeResources();
}

int WebEngineMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (!process_type.empty())
    return -1;

  return WebEngineBrowserMain(main_function_params);
}

content::ContentClient* WebEngineMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<WebEngineContentClient>();
  return content_client_.get();
}

content::ContentBrowserClient*
WebEngineMainDelegate::CreateContentBrowserClient() {
  DCHECK(!browser_client_);
  browser_client_ =
      std::make_unique<WebEngineContentBrowserClient>(std::move(request_));
  return browser_client_.get();
}

content::ContentRendererClient*
WebEngineMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<WebEngineContentRendererClient>();
  return renderer_client_.get();
}
