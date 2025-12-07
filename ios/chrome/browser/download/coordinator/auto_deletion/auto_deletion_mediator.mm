// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_mediator.h"

#import "base/memory/raw_ptr.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/auto_deletion_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/web/public/download/download_task.h"

@implementation AutoDeletionMediator {
  // A pointer to the ApplicationContext::LocatState PrefService.
  raw_ptr<PrefService> _localState;
  // A pointer to the browser object.
  raw_ptr<Browser> _browser;
  // The task that is downloading the content to the device.
  raw_ptr<web::DownloadTask> _downloadTask;
}

- (instancetype)initWithLocalState:(PrefService*)localState
                           browser:(Browser*)browser
                      downloadTask:(web::DownloadTask*)task {
  self = [super init];
  if (self) {
    _localState = localState;
    _browser = browser;
    _downloadTask = task;
  }

  return self;
}

// Enables the auto-deletion feature on the user's device.
- (void)enableAutoDeletion {
  _localState->SetBoolean(prefs::kDownloadAutoDeletionEnabled, true);

  id<AutoDeletionCommands> handler = HandlerForProtocol(
      _browser->GetCommandDispatcher(), AutoDeletionCommands);
  [handler dismissAutoDeletionActionSheet];
  [handler presentAutoDeletionActionSheetWithDownloadTask:_downloadTask];
}

@end
