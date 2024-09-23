// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/tab_lifecycle_mediator.h"

#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"
#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/commerce/model/price_notifications/price_notifications_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_coordinator.h"
#import "ios/chrome/browser/follow/model/follow_tab_helper.h"
#import "ios/chrome/browser/itunes_urls/model/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/lens/model/lens_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_coordinator.h"
#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_prefs.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/prerender/model/prerender_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ssl/model/captive_portal_tab_helper.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/ui/print/print_coordinator.h"
#import "ios/chrome/browser/web/model/annotations/annotations_tab_helper.h"
#import "ios/chrome/browser/web/model/print/print_tab_helper.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper_delegate.h"
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installation_observer.h"
#import "ios/chrome/browser/web_state_list/model/web_state_dependency_installer_bridge.h"
#import "ios/chrome/browser/webui/model/net_export_tab_helper.h"
#import "ios/chrome/browser/webui/model/net_export_tab_helper_delegate.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/device_form_factor.h"

@interface TabLifecycleMediator () <DependencyInstalling>
@end

@implementation TabLifecycleMediator {
  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList {
  if ((self = [super init])) {
    _dependencyInstallerBridge =
        std::make_unique<WebStateDependencyInstallerBridge>(self, webStateList);
  }
  return self;
}

- (void)disconnect {
  // Deleting the installer bridge will cause all web states to have
  // dependencies uninstalled.
  _dependencyInstallerBridge.reset();
}

#pragma mark - DependencyInstalling

- (void)installDependencyForWebState:(web::WebState*)webState {
  // If there is a prerender service, this webstate shouldn't be a prerendered
  // one. (There's no prerender service in incognito, for example).
  DCHECK(!_prerenderService ||
         !_prerenderService->IsWebStatePrerendered(webState));
  // Only realized webstates should have dependencies installed.
  DCHECK(webState->IsRealized());

  DCHECK(_snapshotGeneratorDelegate);
  SnapshotTabHelper::FromWebState(webState)->SetDelegate(
      _snapshotGeneratorDelegate);

  PasswordTabHelper* passwordTabHelper =
      PasswordTabHelper::FromWebState(webState);
  DCHECK(_passwordControllerDelegate);
  DCHECK(_commandDispatcher);
  passwordTabHelper->SetPasswordControllerDelegate(_passwordControllerDelegate);
  passwordTabHelper->SetDispatcher(_commandDispatcher);

  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(webState);
  bottomSheetTabHelper->SetAutofillBottomSheetHandler(
      HandlerForProtocol(_commandDispatcher, AutofillCommands));
  id<PasswordGenerationProvider> generationProvider =
      passwordTabHelper->GetPasswordGenerationProvider();
  bottomSheetTabHelper->SetPasswordGenerationProvider(generationProvider);

  if (ios::provider::IsLensSupported()) {
    LensTabHelper* lensTabHelper = LensTabHelper::FromWebState(webState);
    lensTabHelper->SetLensCommandsHandler(
        HandlerForProtocol(_commandDispatcher, LensCommands));
  }

  DCHECK(_overscrollActionsDelegate);
  OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(
      _overscrollActionsDelegate);

  // DownloadManagerTabHelper cannot function without its delegate.
  DCHECK(_downloadManagerTabHelperDelegate);
  DownloadManagerTabHelper::FromWebState(webState)->SetDelegate(
      _downloadManagerTabHelperDelegate);

  DCHECK(_tabHelperDelegate);
  NetExportTabHelper::GetOrCreateForWebState(webState)->SetDelegate(
      _tabHelperDelegate);

  id<WebContentCommands> webContentsHandler =
      HandlerForProtocol(_commandDispatcher, WebContentCommands);
  DCHECK(webContentsHandler);
  ITunesUrlsHandlerTabHelper::GetOrCreateForWebState(webState)
      ->SetWebContentsHandler(webContentsHandler);
  PassKitTabHelper::GetOrCreateForWebState(webState)->SetWebContentsHandler(
      webContentsHandler);

  DCHECK(_baseViewController);
  AutofillTabHelper* autofillTabHelper =
      AutofillTabHelper::FromWebState(webState);
  autofillTabHelper->SetBaseViewController(_baseViewController);
  id<AutofillCommands> autofillHandler =
      HandlerForProtocol(_commandDispatcher, AutofillCommands);
  autofillTabHelper->SetCommandsHandler(autofillHandler);

  DCHECK(_printCoordinator);
  PrintTabHelper::GetOrCreateForWebState(webState)->set_printer(
      _printCoordinator);

  RepostFormTabHelper::FromWebState(webState)->SetDelegate(_repostFormDelegate);

  FollowTabHelper* followTabHelper = FollowTabHelper::FromWebState(webState);
  if (followTabHelper) {
    followTabHelper->set_help_handler(
        HandlerForProtocol(_commandDispatcher, HelpCommands));
  }

  DCHECK(_tabInsertionBrowserAgent);
  CaptivePortalTabHelper::GetOrCreateForWebState(webState)
      ->SetTabInsertionBrowserAgent(_tabInsertionBrowserAgent);

  NewTabPageTabHelper::FromWebState(webState)->SetDelegate(
      _NTPTabHelperDelegate);

  AnnotationsTabHelper* annotationsTabHelper =
      AnnotationsTabHelper::FromWebState(webState);
  if (annotationsTabHelper) {
    DCHECK(_baseViewController);
    annotationsTabHelper->SetBaseViewController(_baseViewController);
    annotationsTabHelper->SetMiniMapCommands(
        HandlerForProtocol(_commandDispatcher, MiniMapCommands));
    annotationsTabHelper->SetUnitConversionCommands(
        HandlerForProtocol(_commandDispatcher, UnitConversionCommands));

    PrefService* prefs =
        IsHomeCustomizationEnabled()
            ? ProfileIOS::FromBrowserState(webState->GetBrowserState())
                  ->GetPrefs()
            : GetApplicationContext()->GetLocalState();

    if (IsIOSParcelTrackingEnabled() && !IsParcelTrackingDisabled(prefs)) {
      annotationsTabHelper->SetParcelTrackingOptInCommands(
          HandlerForProtocol(_commandDispatcher, ParcelTrackingOptInCommands));
    }
  }

  PriceNotificationsTabHelper* priceNotificationsTabHelper =
      PriceNotificationsTabHelper::FromWebState(webState);
  if (priceNotificationsTabHelper) {
    priceNotificationsTabHelper->SetHelpHandler(
        HandlerForProtocol(_commandDispatcher, HelpCommands));
  }
  AppLauncherTabHelper::FromWebState(webState)->SetBrowserPresentationProvider(
      _appLauncherBrowserPresentationProvider);

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(webState);
  if (contextualPanelTabHelper) {
    id<ContextualSheetCommands> contextualSheetHandler =
        HandlerForProtocol(_commandDispatcher, ContextualSheetCommands);
    contextualPanelTabHelper->SetContextualSheetHandler(contextualSheetHandler);
  }
}

- (void)uninstallDependencyForWebState:(web::WebState*)webState {
  // Only realized webstates should have dependencies uninstalled.
  DCHECK(webState->IsRealized());

  // Remove delegates for tab helpers which may otherwise do bad things during
  // shutdown.
  SnapshotTabHelper::FromWebState(webState)->SetDelegate(nil);

  PasswordTabHelper* passwordTabHelper =
      PasswordTabHelper::FromWebState(webState);
  passwordTabHelper->SetPasswordControllerDelegate(nil);
  passwordTabHelper->SetDispatcher(nil);

  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(webState);
  bottomSheetTabHelper->SetAutofillBottomSheetHandler(nil);

  LensTabHelper* lensTabHelper = LensTabHelper::FromWebState(webState);
  if (lensTabHelper) {
    lensTabHelper->SetLensCommandsHandler(nil);
  }

  OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(nil);

  DownloadManagerTabHelper::FromWebState(webState)->SetDelegate(nil);

  NetExportTabHelper::GetOrCreateForWebState(webState)->SetDelegate(nil);

  AutofillTabHelper* autofillTabHelper =
      AutofillTabHelper::FromWebState(webState);
  autofillTabHelper->SetBaseViewController(nil);
  autofillTabHelper->SetCommandsHandler(nil);

  PrintTabHelper::GetOrCreateForWebState(webState)->set_printer(nil);

  RepostFormTabHelper::FromWebState(webState)->SetDelegate(nil);

  FollowTabHelper* followTabHelper = FollowTabHelper::FromWebState(webState);
  if (followTabHelper) {
    followTabHelper->set_help_handler(nil);
  }

  CaptivePortalTabHelper::GetOrCreateForWebState(webState)
      ->SetTabInsertionBrowserAgent(nil);

  NewTabPageTabHelper::FromWebState(webState)->SetDelegate(nil);

  AnnotationsTabHelper* annotationsTabHelper =
      AnnotationsTabHelper::FromWebState(webState);
  if (annotationsTabHelper) {
    annotationsTabHelper->SetBaseViewController(nil);
    annotationsTabHelper->SetMiniMapCommands(nil);
    annotationsTabHelper->SetUnitConversionCommands(nil);
  }

  PriceNotificationsTabHelper* priceNotificationsTabHelper =
      PriceNotificationsTabHelper::FromWebState(webState);
  if (priceNotificationsTabHelper) {
    priceNotificationsTabHelper->SetHelpHandler(nil);
  }

  AppLauncherTabHelper::FromWebState(webState)->SetBrowserPresentationProvider(
      nil);

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(webState);
  if (contextualPanelTabHelper) {
    contextualPanelTabHelper->SetContextualSheetHandler(nil);
  }
}

@end
