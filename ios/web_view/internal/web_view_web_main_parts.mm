// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_main_parts.h"

#import <string_view>

#import "base/base_paths.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/path_service.h"
#import "base/strings/string_util.h"
#import "base/time/default_clock.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/component_updater/installer_policies/safety_tips_component_installer.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/base/features.h"
#import "components/variations/variations_ids_provider.h"
#import "ios/web/public/webui/web_ui_ios_controller_factory.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/cwv_flags_internal.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#import "ios/web_view/internal/translate/web_view_translate_service.h"
#import "ios/web_view/internal/webui/web_view_web_ui_ios_controller_factory.h"
#import "ios/web_view/public/cwv_global_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/resource/resource_scale_factor.h"
#import "ui/display/screen.h"

#if DCHECK_IS_ON()
#import "ui/display/screen_base.h"
#endif

namespace {
std::string MakeFeaturesString(
    const std::vector<const base::Feature*>& features) {
  std::vector<std::string_view> feature_names;
  for (const auto* feature : features) {
    feature_names.push_back(feature->name);
  }
  return base::JoinString(feature_names, ",");
}
}  // namespace

namespace ios_web_view {

WebViewWebMainParts::WebViewWebMainParts()
    : screen_(std::make_unique<display::ScopedNativeScreen>()) {}

WebViewWebMainParts::~WebViewWebMainParts() {
#if DCHECK_IS_ON()
  // Make sure that all display observers are removed at the end.
  display::ScreenBase* screen =
      static_cast<display::ScreenBase*>(display::Screen::Get());
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

  // TODO: crbug.com/442849530 - Use VariationsNetworkClock instead of
  // base::DefaultClock.
  variations::VariationsIdsProvider::CreateInstance(
      variations::VariationsIdsProvider::Mode::kUseSignedInState,
      std::make_unique<base::DefaultClock>());

  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);

  std::vector<const base::Feature*> enabled_features = {
      &autofill::features::kAutofillUpstream,

      &syncer::kSyncPasswordCleanUpAccidentalBatchDeletions,
  };
  std::vector<const base::Feature*> disabled_features;
  if ([CWVGlobalState sharedInstance].autofillAcrossIframesEnabled) {
    enabled_features.push_back(&autofill::features::kAutofillAcrossIframesIos);
  } else {
    disabled_features.push_back(&autofill::features::kAutofillAcrossIframesIos);
  }
  feature_list->InitFromCommandLine(
      /*enable_features=*/MakeFeaturesString(enabled_features),
      /*disable_features=*/MakeFeaturesString(disabled_features));
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
