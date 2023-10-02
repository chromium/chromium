// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/tab_lifecycle_mediator.h"

#import "ios/chrome/browser/autofill/autofill_tab_helper.h"
#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/commerce/model/price_notifications/price_notifications_iph_presenter.h"
#import "ios/chrome/browser/commerce/model/price_notifications/price_notifications_tab_helper.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/chrome/browser/follow/follow_iph_presenter.h"
#import "ios/chrome/browser/follow/follow_tab_helper.h"
#import "ios/chrome/browser/itunes_urls/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/lens/lens_tab_helper.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/overscroll_actions/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/autofill_bottom_sheet_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/mini_map_commands.h"
#import "ios/chrome/browser/shared/public/commands/parcel_tracking_opt_in_commands.h"
#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ssl/captive_portal_tab_helper.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"
#import "ios/chrome/browser/ui/print/print_coordinator.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_mediator.h"
#import "ios/chrome/browser/web/annotations/annotations_tab_helper.h"
#import "ios/chrome/browser/web/print/print_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper.h"
#import "ios/chrome/browser/web/repost_form_tab_helper_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installation_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"
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
  if (self = [super init]) {
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
      HandlerForProtocol(_commandDispatcher, AutofillBottomSheetCommands));

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
  NetExportTabHelper::FromWebState(webState)->SetDelegate(_tabHelperDelegate);

  id<WebContentCommands> webContentsHandler =
      HandlerForProtocol(_commandDispatcher, WebContentCommands);
  DCHECK(webContentsHandler);
  ITunesUrlsHandlerTabHelper::FromWebState(webState)->SetWebContentsHandler(
      webContentsHandler);
  PassKitTabHelper::FromWebState(webState)->SetWebContentsHandler(
      webContentsHandler);

  DCHECK(_baseViewController);
  AutofillTabHelper::FromWebState(webState)->SetBaseViewController(
      _baseViewController);

  DCHECK(_printCoordinator);
  PrintTabHelper::FromWebState(webState)->set_printer(_printCoordinator);

  RepostFormTabHelper::FromWebState(webState)->SetDelegate(_repostFormDelegate);

  FollowTabHelper* followTabHelper = FollowTabHelper::FromWebState(webState);
  if (followTabHelper) {
    DCHECK(_followIPHPresenter);
    followTabHelper->set_follow_iph_presenter(_followIPHPresenter);
  }

  DCHECK(_tabInsertionBrowserAgent);
  CaptivePortalTabHelper::FromWebState(webState)->SetTabInsertionBrowserAgent(
      _tabInsertionBrowserAgent);

  NewTabPageTabHelper::FromWebState(webState)->SetDelegate(
      _NTPTabHelperDelegate);

  AnnotationsTabHelper* annotationsTabHelper =
      AnnotationsTabHelper::FromWebState(webState);
  if (annotationsTabHelper) {
    DCHECK(_baseViewController);
    annotationsTabHelper->SetBaseViewController(_baseViewController);
    annotationsTabHelper->SetMiniMapCommands(
        HandlerForProtocol(_commandDispatcher, MiniMapCommands));
    if (IsIOSParcelTrackingEnabled()) {
      annotationsTabHelper->SetParcelTrackingOptInCommands(
          HandlerForProtocol(_commandDispatcher, ParcelTrackingOptInCommands));
    }
  }

  PriceNotificationsTabHelper* priceNotificationsTabHelper =
      PriceNotificationsTabHelper::FromWebState(webState);
  if (priceNotificationsTabHelper) {
    DCHECK(_priceNotificationsIPHPresenter);
    priceNotificationsTabHelper->SetPriceNotificationsIPHPresenter(
        _priceNotificationsIPHPresenter);
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

  NetExportTabHelper::FromWebState(webState)->SetDelegate(nil);

  AutofillTabHelper::FromWebState(webState)->SetBaseViewController(nil);

  PrintTabHelper::FromWebState(webState)->set_printer(nil);

  RepostFormTabHelper::FromWebState(webState)->SetDelegate(nil);

  FollowTabHelper* followTabHelper = FollowTabHelper::FromWebState(webState);
  if (followTabHelper) {
    followTabHelper->set_follow_iph_presenter(nil);
  }

  CaptivePortalTabHelper::FromWebState(webState)->SetTabInsertionBrowserAgent(
      nil);

  NewTabPageTabHelper::FromWebState(webState)->SetDelegate(nil);

  AnnotationsTabHelper* annotationsTabHelper =
      AnnotationsTabHelper::FromWebState(webState);
  if (annotationsTabHelper) {
    annotationsTabHelper->SetBaseViewController(nil);
    annotationsTabHelper->SetMiniMapCommands(nil);
  }

  PriceNotificationsTabHelper* priceNotificationsTabHelper =
      PriceNotificationsTabHelper::FromWebState(webState);
  if (priceNotificationsTabHelper) {
    priceNotificationsTabHelper->SetPriceNotificationsIPHPresenter(nil);
  }
}

@end
