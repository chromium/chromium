// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/tab_lifecycle_mediator.h"

#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/chrome/browser/itunes_urls/itunes_urls_handler_tab_helper.h"
#import "ios/chrome/browser/overscroll_actions/overscroll_actions_tab_helper.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/web_content_commands.h"
#import "ios/chrome/browser/ui/download/download_manager_coordinator.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installation_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installer_bridge.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/webui/net_export_tab_helper.h"
#import "ios/chrome/browser/webui/net_export_tab_helper_delegate.h"
#import "ios/web/public/deprecated/crw_web_controller_util.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabLifecycleMediator () <DependencyInstalling>
@end

@implementation TabLifecycleMediator {
  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateDependencyInstallerBridge> _dependencyInstallerBridge;

  // Delegate object for many tab helpers.
  __weak id<CommonTabHelperDelegate> _delegate;

  // Delegate object for Snapshot Generator.
  __weak id<SnapshotGeneratorDelegate> _snapshotGeneratorDelegate;

  // Other tab helper dependencies.
  PrerenderService* _prerenderService;
  __weak SideSwipeController* _sideSwipeController;
  __weak DownloadManagerCoordinator* _downloadManagerCoordinator;
  __weak UIViewController* _baseViewController;
  __weak CommandDispatcher* _commandDispatcher;
  __weak id<NetExportTabHelperDelegate> _tabHelperDelegate;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                            delegate:(id<CommonTabHelperDelegate>)delegate
           snapshotGeneratorDelegate:
               (id<SnapshotGeneratorDelegate>)snapshotGeneratorDelegate
                        dependencies:(TabLifecycleDependencies)dependencies {
  if (self = [super init]) {
    _prerenderService = dependencies.prerenderService;
    _sideSwipeController = dependencies.sideSwipeController;
    _downloadManagerCoordinator = dependencies.downloadManagerCoordinator;
    _baseViewController = dependencies.baseViewController;
    _commandDispatcher = dependencies.commandDispatcher;
    _tabHelperDelegate = dependencies.tabHelperDelegate;

    // Set the delegate before any of the dependency observers, because they
    // will do delegate installation on creation.
    _delegate = delegate;
    _snapshotGeneratorDelegate = snapshotGeneratorDelegate;

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

  SnapshotTabHelper::FromWebState(webState)->SetDelegate(
      _snapshotGeneratorDelegate);

  if (PasswordTabHelper* passwordTabHelper =
          PasswordTabHelper::FromWebState(webState)) {
    passwordTabHelper->SetBaseViewController(_baseViewController);
    passwordTabHelper->SetPasswordControllerDelegate(_delegate);
    passwordTabHelper->SetDispatcher(_commandDispatcher);
  }

  OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(_delegate);

  web_deprecated::SetSwipeRecognizerProvider(webState, _sideSwipeController);

  // DownloadManagerTabHelper cannot function without its delegate.
  DCHECK(_downloadManagerCoordinator);
  DownloadManagerTabHelper::FromWebState(webState)->SetDelegate(
      _downloadManagerCoordinator);

  NetExportTabHelper::FromWebState(webState)->SetDelegate(_tabHelperDelegate);

  id<WebContentCommands> webContentsHandler =
      HandlerForProtocol(_commandDispatcher, WebContentCommands);
  ITunesUrlsHandlerTabHelper::FromWebState(webState)->SetWebContentsHandler(
      webContentsHandler);
  PassKitTabHelper::FromWebState(webState)->SetWebContentsHandler(
      webContentsHandler);
}

- (void)uninstallDependencyForWebState:(web::WebState*)webState {
  // Only realized webstates should have dependencies uninstalled.
  DCHECK(webState->IsRealized());

  // Remove delegates for tab helpers which may otherwise do bad things during
  // shutdown.
  SnapshotTabHelper::FromWebState(webState)->SetDelegate(nil);

  if (PasswordTabHelper* passwordTabHelper =
          PasswordTabHelper::FromWebState(webState)) {
    passwordTabHelper->SetBaseViewController(nil);
    passwordTabHelper->SetPasswordControllerDelegate(nil);
    passwordTabHelper->SetDispatcher(nil);
  }

  OverscrollActionsTabHelper::FromWebState(webState)->SetDelegate(nil);

  web_deprecated::SetSwipeRecognizerProvider(webState, nil);

  DownloadManagerTabHelper::FromWebState(webState)->SetDelegate(nil);

  NetExportTabHelper::FromWebState(webState)->SetDelegate(nil);
}

@end
