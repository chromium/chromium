// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_main_parts.h"

#include "base/base_paths.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/component_updater/installer_policies/safety_tips_component_installer.h"
#include "components/password_manager/core/common/password_manager_features.h"
#import "components/signin/public/base/signin_switches.h"
#include "components/sync/base/features.h"
#include "components/variations/variations_ids_provider.h"
#include "ios/web/public/webui/web_ui_ios_controller_factory.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/cwv_flags_internal.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#include "ios/web_view/internal/translate/web_view_translate_service.h"
#include "ios/web_view/internal/webui/web_view_web_ui_ios_controller_factory.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/base/resource/resource_scale_factor.h"

#if DCHECK_IS_ON()
#include "ui/display/screen_base.h"
#endif

namespace ios_web_view {

WebViewWebMainParts::WebViewWebMainParts() = default;

WebViewWebMainParts::~WebViewWebMainParts() {
#if DCHECK_IS_ON()
  // The screen object is never deleted on IOS. Make sure that all display
  // observers are removed at the end.
  display::ScreenBase* screen =
      static_cast<display::ScreenBase*>(display::Screen::GetScreen());
  DCHECK(!screen->HasDisplayObservers());
#endif
}

void WebViewWebMainParts::PreCreateMainMessageLoop() {
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

  // Flags are converted here to ensure it is set before being read by others.
  [[CWVFlags sharedInstance] convertFlagsToCommandLineSwitches];

  ApplicationContext::GetInstance()->PreCreateThreads();

  variations::VariationsIdsProvider::Create(
      variations::VariationsIdsProvider::Mode::kUseSignedInState);

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  std::string enable_features = base::JoinString(
      {
          autofill::features::kAutofillUpstream.name,
          syncer::kSyncPasswordCleanUpAccidentalBatchDeletions.name,
      },
      ",");
  std::string disabled_features;
  feature_list->InitFromCommandLine(
      /*enable_features=*/enable_features,
      /*disable_features=*/disabled_features);
  base::FeatureList::SetInstance(std::move(feature_list));
}

void WebViewWebMainParts::PostCreateThreads() {
  ApplicationContext::GetInstance()->PostCreateThreads();
}

void WebViewWebMainParts::PreMainMessageLoopRun() {
  WebViewTranslateService::GetInstance()->Initialize();

  web::WebUIIOSControllerFactory::RegisterFactory(
      WebViewWebUIIOSControllerFactory::GetInstance());

  component_updater::ComponentUpdateService* cus =
      ApplicationContext::GetInstance()->GetComponentUpdateService();
  RegisterSafetyTipsComponent(cus);
}

void WebViewWebMainParts::PostMainMessageLoopRun() {
  ApplicationContext::GetInstance()->ShutdownSafeBrowsingServiceIfNecessary();

  // CWVWebViewConfiguration must destroy its WebViewBrowserStates before the
  // threads are stopped by ApplicationContext.
  [CWVWebViewConfiguration shutDown];

  // Translate must be shutdown AFTER CWVWebViewConfiguration since translate
  // may receive final callbacks during webstate shutdowns.
  WebViewTranslateService::GetInstance()->Shutdown();

  ApplicationContext::GetInstance()->SaveState();
}

void WebViewWebMainParts::PostDestroyThreads() {
  ApplicationContext::GetInstance()->PostDestroyThreads();
}

void WebViewWebMainParts::LoadNonScalableResources() {
  base::FilePath pak_file;
  base::PathService::Get(base::DIR_ASSETS, &pak_file);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("web_view_resources.pak"));
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  resource_bundle.AddDataPackFromPath(pak_file, ui::kScaleFactorNone);
}

void WebViewWebMainParts::LoadScalableResources() {
  ui::ResourceBundle& resource_bundle = ui::ResourceBundle::GetSharedInstance();
  if (ui::IsScaleFactorSupported(ui::k100Percent)) {
    base::FilePath pak_file_100;
    base::PathService::Get(base::DIR_ASSETS, &pak_file_100);
    pak_file_100 =
        pak_file_100.Append(FILE_PATH_LITERAL("web_view_100_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file_100, ui::k100Percent);
  }

  if (ui::IsScaleFactorSupported(ui::k200Percent)) {
    base::FilePath pak_file_200;
    base::PathService::Get(base::DIR_ASSETS, &pak_file_200);
    pak_file_200 =
        pak_file_200.Append(FILE_PATH_LITERAL("web_view_200_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file_200, ui::k200Percent);
  }

  if (ui::IsScaleFactorSupported(ui::k300Percent)) {
    base::FilePath pak_file_300;
    base::PathService::Get(base::DIR_ASSETS, &pak_file_300);
    pak_file_300 =
        pak_file_300.Append(FILE_PATH_LITERAL("web_view_300_percent.pak"));
    resource_bundle.AddDataPackFromPath(pak_file_300, ui::k300Percent);
  }
}

}  // namespace ios_web_view
