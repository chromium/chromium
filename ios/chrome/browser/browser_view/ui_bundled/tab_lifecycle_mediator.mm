// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_view/ui_bundled/tab_lifecycle_mediator.h"

#import "ios/chrome/browser/app_launcher/model/app_launcher_tab_helper.h"
#import "ios/chrome/browser/autofill/model/autofill_tab_helper.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/browser_container/model/edit_menu_tab_helper.h"
#import "ios/chrome/browser/commerce/model/price_notifications/price_notifications_tab_helper.h"
#import "ios/chrome/browser/contextual_panel/model/contextual_panel_tab_helper.h"
#import "ios/chrome/browser/download/coordinator/download_manager_coordinator.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/model/pass_kit_tab_helper.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_tab_helper.h"
#import "ios/chrome/browser/find_in_page/model/find_tab_helper.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/itunes_urls/model/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/lens/model/lens_tab_helper.h"
#import "ios/chrome/browser/mini_map/model/mini_map_tab_helper.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/overscroll_actions/model/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/prerender/model/prerender_tab_helper.h"
#import "ios/chrome/browser/print/coordinator/print_coordinator.h"
#import "ios/chrome/browser/reader_mode/model/features.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_tab_helper.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"
#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/data_controls_commands.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/shared/public/commands/reader_mode_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/commands/unit_conversion_commands.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mediator.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/ssl/model/captive_portal_tab_helper.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer.h"
#import "ios/chrome/browser/tabs/model/tabs_dependency_installer_bridge.h"
#import "ios/chrome/browser/web/model/annotations/annotations_tab_helper.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"
#import "ios/chrome/browser/web/model/print/print_tab_helper.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/model/repost_form_tab_helper_delegate.h"
#import "ios/chrome/browser/webui/model/net_export_tab_helper.h"
#import "ios/chrome/browser/webui/model/net_export_tab_helper_delegate.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ui/base/device_form_factor.h"

@interface TabLifecycleMediator () <TabsDependencyInstalling>

// The source browser.
@property(nonatomic, assign) Browser* browser;

@end

@implementation TabLifecycleMediator {
  // Bridge to observe the web state list from Objective-C.
  TabsDependencyInstallerBridge _dependencyInstallerBridge;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    _dependencyInstallerBridge.StartObserving(
        self, browser, TabsDependencyInstaller::Policy::kOnlyRealized);
    _browser = browser;
  }
  return self;
}

- (void)disconnect {
  // Stop observing the WebStateList before destroying the bridge object.
  _dependencyInstallerBridge.StopObserving();
}

#pragma mark - TabsDependencyInstalling

- (void)webStateInserted:(web::WebState*)webState {
  // Only realized webstates should have dependencies installed.
  DCHECK(webState->IsRealized());

  // The WebState must not be used for prerendering (i.e. it must not have a
  // PrerenderTabHelper attached).
  DCHECK(!PrerenderTabHelper::FromWebState(webState));

  DCHECK(_snapshotGeneratorDelegate);
  SnapshotTabHelper* snapshotTabHelper =
      SnapshotTabHelper::FromWebState(webState);
  if (snapshotTabHelper) {
    snapshotTabHelper->SetDelegate(_snapshotGeneratorDelegate);
  }

  PasswordTabHelper* passwordTabHelper =
      PasswordTabHelper::FromWebState(webState);
  AutofillTabHelper* autofillTabHelper =
      AutofillTabHelper::FromWebState(webState);
  if (passwordTabHelper && autofillTabHelper) {
    FormSuggestionTabHelper::CreateForWebState(webState, @[
      passwordTabHelper->GetSuggestionProvider(),
      autofillTabHelper->GetSuggestionProvider()
    ]);
  }

  if (passwordTabHelper) {
    DCHECK(_passwordControllerDelegate);
    DCHECK(_commandDispatcher);
    passwordTabHelper->SetPasswordControllerDelegate(
        _passwordControllerDelegate);
    passwordTabHelper->SetDispatcher(_commandDispatcher);
  }

  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(webState);
  if (bottomSheetTabHelper) {
    bottomSheetTabHelper->SetAutofillBottomSheetHandler(
        HandlerForProtocol(_commandDispatcher, AutofillCommands));
    id<PasswordGenerationProvider> generationProvider =
        passwordTabHelper->GetPasswordGenerationProvider();
    bottomSheetTabHelper->SetPasswordGenerationProvider(generationProvider);
  }

  SupervisedUserErrorContainer* supervisedUserErrorContainer =
      SupervisedUserErrorContainer::FromWebState(webState);
  if (supervisedUserErrorContainer) {
    supervisedUserErrorContainer->SetParentAccessBottomSheetHandler(
        HandlerForProtocol(_commandDispatcher, ParentAccessCommands));
  }

  if (ios::provider::IsLensSupported()) {
    LensTabHelper* lensTabHelper = LensTabHelper::FromWebState(webState);
    if (lensTabHelper) {
      lensTabHelper->SetLensCommandsHandler(
          HandlerForProtocol(_commandDispatcher, LensCommands));
    }
  }

  DCHECK(_overscrollActionsDelegate);
  OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(
      _overscrollActionsDelegate);

  data_controls::DataControlsTabHelper::GetOrCreateForWebState(webState)
      ->SetDataControlsCommandsHandler(
          HandlerForProtocol(_commandDispatcher, DataControlsCommands));
  data_controls::DataControlsTabHelper::GetOrCreateForWebState(webState)
      ->SetSnackbarHandler(
          static_cast<id<SnackbarCommands>>(_commandDispatcher));

  // DownloadManagerTabHelper cannot function without its delegate.
  DCHECK(_downloadManagerTabHelperDelegate);
  DownloadManagerTabHelper::FromWebState(webState)->SetDelegate(
      _downloadManagerTabHelperDelegate);
  DownloadManagerTabHelper::FromWebState(webState)->SetSnackbarHandler(
      static_cast<id<SnackbarCommands>>(_commandDispatcher));

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
  if (autofillTabHelper) {
    autofillTabHelper->SetBaseViewController(_baseViewController);
    id<AutofillCommands> autofillHandler =
        HandlerForProtocol(_commandDispatcher, AutofillCommands);
    autofillTabHelper->SetAutofillHandler(autofillHandler);
    autofillTabHelper->SetSnackbarHandler(
        static_cast<id<SnackbarCommands>>(_commandDispatcher));
  }

  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(webState);

  if (readerModeTabHelper) {
    readerModeTabHelper->SetReaderModeHandler(
        HandlerForProtocol(_commandDispatcher, ReaderModeCommands));
  }

  DCHECK(_printCoordinator);
  PrintTabHelper::GetOrCreateForWebState(webState)->set_printer(
      _printCoordinator);

  RepostFormTabHelper::FromWebState(webState)->SetDelegate(_repostFormDelegate);

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
  }

  MiniMapTabHelper* miniMapTabHelper = MiniMapTabHelper::FromWebState(webState);
  if (miniMapTabHelper) {
    miniMapTabHelper->SetMiniMapCommands(
        HandlerForProtocol(_commandDispatcher, MiniMapCommands));
  }

  PriceNotificationsTabHelper* priceNotificationsTabHelper =
      PriceNotificationsTabHelper::FromWebState(webState);
  if (priceNotificationsTabHelper) {
    priceNotificationsTabHelper->SetHelpHandler(
        HandlerForProtocol(_commandDispatcher, HelpCommands));
  }

  AppLauncherTabHelper* appLauncherTabHelper =
      AppLauncherTabHelper::FromWebState(webState);
  if (appLauncherTabHelper) {
    appLauncherTabHelper->SetBrowserPresentationProvider(
        _appLauncherBrowserPresentationProvider);
  }

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(webState);
  if (contextualPanelTabHelper) {
    id<ContextualSheetCommands> contextualSheetHandler =
        HandlerForProtocol(_commandDispatcher, ContextualSheetCommands);
    contextualPanelTabHelper->SetContextualSheetHandler(contextualSheetHandler);
  }

  EditMenuTabHelper* editMenuTabHelper =
      EditMenuTabHelper::FromWebState(webState);
  if (editMenuTabHelper) {
    editMenuTabHelper->SetEditMenuBuilder(self.editMenuBuilder);
  }

  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
  if (BWGTabHelper) {
    id<BWGCommands> BWGCommandsHandler =
        HandlerForProtocol(_commandDispatcher, BWGCommands);
    BWGTabHelper->SetBwgCommandsHandler(BWGCommandsHandler);

    // TODO(crbug.com/448157489): Remove this or refactor to
    // `HandlerForProtocol`.
    if (IsAskGeminiSnackbarEnabled() || IsWebPageReportedImagesSheetEnabled()) {
      BWGTabHelper->SetSnackbarCommandsHandler(
          static_cast<id<SnackbarCommands>>(_commandDispatcher));
    }

    if (IsAskGeminiChipEnabled()) {
      BWGTabHelper->SetLocationBarBadgeCommandsHandler(
          id<LocationBarBadgeCommands>(_commandDispatcher));
    }
  }

  FindTabHelper* findTabHelper = FindTabHelper::FromWebState(webState);
  if (findTabHelper) {
    FullscreenController* fullscreenController =
        FullscreenController::FromBrowser(self.browser);
    findTabHelper->SetFullscreenController(fullscreenController);
  }

  if (base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu)) {
    ChooseFileTabHelper* chooseFileTabHelper =
        ChooseFileTabHelper::FromWebState(webState);
    if (chooseFileTabHelper) {
      chooseFileTabHelper->SetFileUploadPanelHandler(
          HandlerForProtocol(_commandDispatcher, FileUploadPanelCommands));
    }
  }
}

- (void)webStateRemoved:(web::WebState*)webState {
  // Only realized webstates should have dependencies uninstalled.
  DCHECK(webState->IsRealized());

  // Remove delegates for tab helpers which may otherwise do bad things during
  // shutdown.
  SnapshotTabHelper* snapshotTabHelper =
      SnapshotTabHelper::FromWebState(webState);
  if (snapshotTabHelper) {
    snapshotTabHelper->SetDelegate(nil);
  }

  PasswordTabHelper* passwordTabHelper =
      PasswordTabHelper::FromWebState(webState);
  if (passwordTabHelper) {
    passwordTabHelper->SetPasswordControllerDelegate(nil);
    passwordTabHelper->SetDispatcher(nil);
  }

  AutofillBottomSheetTabHelper* bottomSheetTabHelper =
      AutofillBottomSheetTabHelper::FromWebState(webState);
  if (bottomSheetTabHelper) {
    bottomSheetTabHelper->SetAutofillBottomSheetHandler(nil);
  }

  SupervisedUserErrorContainer* supervisedUserErrorContainer =
      SupervisedUserErrorContainer::FromWebState(webState);
  if (supervisedUserErrorContainer) {
    supervisedUserErrorContainer->SetParentAccessBottomSheetHandler(nil);
  }

  LensTabHelper* lensTabHelper = LensTabHelper::FromWebState(webState);
  if (lensTabHelper) {
    lensTabHelper->SetLensCommandsHandler(nil);
  }

  OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(nil);

  data_controls::DataControlsTabHelper::GetOrCreateForWebState(webState)
      ->SetDataControlsCommandsHandler(nil);
  data_controls::DataControlsTabHelper::GetOrCreateForWebState(webState)
      ->SetSnackbarHandler(nil);

  DownloadManagerTabHelper::FromWebState(webState)->SetDelegate(nil);
  DownloadManagerTabHelper::FromWebState(webState)->SetSnackbarHandler(nil);

  NetExportTabHelper::GetOrCreateForWebState(webState)->SetDelegate(nil);

  AutofillTabHelper* autofillTabHelper =
      AutofillTabHelper::FromWebState(webState);
  if (autofillTabHelper) {
    autofillTabHelper->SetBaseViewController(nil);
    autofillTabHelper->SetAutofillHandler(nil);
    autofillTabHelper->SetSnackbarHandler(nil);
  }

  ReaderModeTabHelper* readerModeTabHelper =
      ReaderModeTabHelper::FromWebState(webState);
  if (readerModeTabHelper) {
    readerModeTabHelper->SetReaderModeHandler(nil);
  }

  PrintTabHelper::GetOrCreateForWebState(webState)->set_printer(nil);

  RepostFormTabHelper::FromWebState(webState)->SetDelegate(nil);

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

  MiniMapTabHelper* miniMapTabHelper = MiniMapTabHelper::FromWebState(webState);
  if (miniMapTabHelper) {
    miniMapTabHelper->SetMiniMapCommands(nil);
  }

  PriceNotificationsTabHelper* priceNotificationsTabHelper =
      PriceNotificationsTabHelper::FromWebState(webState);
  if (priceNotificationsTabHelper) {
    priceNotificationsTabHelper->SetHelpHandler(nil);
  }

  AppLauncherTabHelper* appLauncherTabHelper =
      AppLauncherTabHelper::FromWebState(webState);
  if (appLauncherTabHelper) {
    appLauncherTabHelper->SetBrowserPresentationProvider(nil);
  }

  ContextualPanelTabHelper* contextualPanelTabHelper =
      ContextualPanelTabHelper::FromWebState(webState);
  if (contextualPanelTabHelper) {
    contextualPanelTabHelper->SetContextualSheetHandler(nil);
  }

  EditMenuTabHelper* editMenuTabHelper =
      EditMenuTabHelper::FromWebState(webState);
  if (editMenuTabHelper) {
    editMenuTabHelper->SetEditMenuBuilder(nil);
  }

  FormSuggestionTabHelper::RemoveFromWebState(webState);

  BwgTabHelper* BWGTabHelper = BwgTabHelper::FromWebState(webState);
  if (BWGTabHelper) {
    BWGTabHelper->SetBwgCommandsHandler(nil);
    if (IsAskGeminiSnackbarEnabled()) {
      BWGTabHelper->SetSnackbarCommandsHandler(nil);
    }
    if (IsAskGeminiChipEnabled()) {
      BWGTabHelper->SetLocationBarBadgeCommandsHandler(nil);
    }
  }

  FindTabHelper* findTabHelper = FindTabHelper::FromWebState(webState);
  if (findTabHelper) {
    findTabHelper->SetFullscreenController(nullptr);
  }

  if (base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu)) {
    ChooseFileTabHelper* chooseFileTabHelper =
        ChooseFileTabHelper::FromWebState(webState);
    if (chooseFileTabHelper) {
      chooseFileTabHelper->SetFileUploadPanelHandler(nil);
    }
  }
}

- (void)webStateDeleted:(web::WebState*)webState {
  // Nothing to do.
}

- (void)newWebStateActivated:(web::WebState*)newActive
           oldActiveWebState:(web::WebState*)oldActive {
  // Nothing to do.
}

@end
