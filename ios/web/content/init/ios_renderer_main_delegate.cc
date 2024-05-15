// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/content/init/ios_renderer_main_delegate.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "content/public/app/content_main.h"
#include "content/public/renderer/content_renderer_client.h"
#include "ios/web/content/init/ios_content_client.h"
#include "ios/web/content/init/ios_content_renderer_client.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#define IOS_INIT_EXPORT __attribute__((visibility("default")))

extern "C" IOS_INIT_EXPORT int ChildProcessMain(int argc, const char** argv) {
  // Create this here since it's needed to start the crash handler.
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  web::IOSRendererMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(std::move(params));
}

namespace web {

IOSRendererMainDelegate::IOSRendererMainDelegate() = default;
IOSRendererMainDelegate::~IOSRendererMainDelegate() = default;

content::ContentClient* IOSRendererMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<IOSContentClient>();
  return content_client_.get();
}
content::ContentRendererClient*
IOSRendererMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<IOSContentRendererClient>();
  return renderer_client_.get();
}

void IOSRendererMainDelegate::PreSandboxStartup() {
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      std::string(), nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);

  base::FilePath resources_pack_path;
  base::PathService::Get(base::DIR_ASSETS, &resources_pack_path);
  resources_pack_path =
      resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pack_path, ui::kScaleFactorNone);
}

}  // namespace web
