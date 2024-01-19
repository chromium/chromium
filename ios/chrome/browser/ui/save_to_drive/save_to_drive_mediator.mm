// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_mediator.h"

#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/drive/model/drive_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/download/download_task_observer_bridge.h"
#import "ios/web/public/web_state_observer_bridge.h"

@interface SaveToDriveMediator () <CRWWebStateObserver, CRWDownloadTaskObserver>

@end

@implementation SaveToDriveMediator {
  raw_ptr<web::DownloadTask> _downloadTask;
  std::unique_ptr<web::DownloadTaskObserverBridge> _downloadTaskObserverBridge;
  raw_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  id<SaveToDriveCommands> _saveToDriveCommandsHandler;
}

- (instancetype)initWithDownloadTask:(web::DownloadTask*)downloadTask
          saveToDriveCommandsHandler:
              (id<SaveToDriveCommands>)saveToDriveCommandsHandler {
  self = [super init];
  if (self) {
    _downloadTask = downloadTask;
    _downloadTaskObserverBridge =
        std::make_unique<web::DownloadTaskObserverBridge>(self);
    _downloadTask->AddObserver(_downloadTaskObserverBridge.get());
    _webState = downloadTask->GetWebState();
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());
    _saveToDriveCommandsHandler = saveToDriveCommandsHandler;
  }
  return self;
}

- (void)disconnect {
  if (_downloadTask) {
    _downloadTask->RemoveObserver(_downloadTaskObserverBridge.get());
  }
  _downloadTask = nullptr;
  _downloadTaskObserverBridge = nullptr;
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
  _webState = nullptr;
  _webStateObserverBridge = nullptr;
  _saveToDriveCommandsHandler = nil;
}

- (void)startDownloadAndSaveToDriveWithIdentity:(id<SystemIdentity>)identity {
  if (!_downloadTask || !_webState) {
    return;
  }
  DriveTabHelper* driveTabHelper = DriveTabHelper::FromWebState(_webState);
  DownloadManagerTabHelper* downloadManagerTabHelper =
      DownloadManagerTabHelper::FromWebState(_webState);
  driveTabHelper->AddDownloadToSaveToDrive(_downloadTask, identity);
  downloadManagerTabHelper->StartDownload(_downloadTask);
}

#pragma mark - CRWDownloadTaskObserver

- (void)downloadDestroyed:(web::DownloadTask*)task {
  CHECK_EQ(task, _downloadTask);
  task->RemoveObserver(_downloadTaskObserverBridge.get());
  _downloadTaskObserverBridge = nullptr;
  _downloadTask = nullptr;
  [_saveToDriveCommandsHandler hideSaveToDrive];
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasHidden:(web::WebState*)webState {
  [_saveToDriveCommandsHandler hideSaveToDrive];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  CHECK_EQ(webState, _webState);
  webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge = nullptr;
  _webState = nullptr;
  [_saveToDriveCommandsHandler hideSaveToDrive];
}

@end
