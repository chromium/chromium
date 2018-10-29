// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_main_parts.h"

#include "base/base_paths.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "ios/web_view/cwv_web_view_features.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#include "ios/web_view/internal/translate/web_view_translate_service.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewWebMainParts::WebViewWebMainParts() {}

WebViewWebMainParts::~WebViewWebMainParts() = default;

void WebViewWebMainParts::PreMainMessageLoopStart() {
  l10n_util::OverrideLocaleWithCocoaLocale();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      std::string(), nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  LoadNonScalableResources();
  LoadScalableResources();
}

void WebViewWebMainParts::PreCreateThreads() {
  // Initialize local state.
  PrefService* local_state = ApplicationContext::GetInstance()->GetLocalState();
  DCHECK(local_state);

  ApplicationContext::GetInstance()->PreCreateThreads();

#if BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->InitializeFromCommandLine(
      /*enable_features=*/"",
      // TODO(crbug.com/873790): Figure out if USER_CONSENTS need to be enabled.
      /*disable_features=*/"SyncUserConsentSeparateType");
  base::FeatureList::SetInstance(std::move(feature_list));
#endif  // BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)
}

void WebViewWebMainParts::PreMainMessageLoopRun() {
  WebViewTranslateService::GetInstance()->Initialize();

#if BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)
  ContentSettingsPattern::SetNonWildcardDomainNonPortSchemes(
      /*schemes=*/nullptr, 0);
#endif  // BUILDFLAG(IOS_WEB_VIEW_ENABLE_SYNC)
}

void WebViewWebMainParts::PostMainMessageLoopRun() {
  WebViewTranslateService::GetInstance()->Shutdown();
  ApplicationContext::GetInstance()->SaveState();
  [CWVWebViewConfiguration shutDown];
}

void WebViewWebMainParts::PostDestroyThreads() {
  ApplicationContext::GetInstance()->PostDestroyThreads();
}

void WebViewWebMainParts::LoadNonScalableResources() {
  base::FilePath pak_file;
  base::PathService::Get(base::DIR_MODULE, &pak_file);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("web_view_resources.pak"));
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  resource_bundle.AddDataPackFromPath(pak_file, ui::SCALE_FACTOR_NONE);
}

void WebViewWebMainParts::LoadScalableResources() {
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_100P)) {
    base::FilePath pak_file_100;
    base::PathService::Get(base::DIR_MODULE, &pak_file_100);
    pak_file_100 =
        pak_file_100.Append(FILE_PATH_LITERAL("web_view_100_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file_100, ui::SCALE_FACTOR_100P);
  }

  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_200P)) {
    base::FilePath pak_file_200;
    base::PathService::Get(base::DIR_MODULE, &pak_file_200);
    pak_file_200 =
        pak_file_200.Append(FILE_PATH_LITERAL("web_view_200_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file_200, ui::SCALE_FACTOR_200P);
  }

  if (ui::ResourceBundle::IsScaleFactorSupported(ui::SCALE_FACTOR_300P)) {
    base::FilePath pak_file_300;
    base::PathService::Get(base::DIR_MODULE, &pak_file_300);
    pak_file_300 =
        pak_file_300.Append(FILE_PATH_LITERAL("web_view_300_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file_300, ui::SCALE_FACTOR_300P);
  }
}

}  // namespace ios_web_view
