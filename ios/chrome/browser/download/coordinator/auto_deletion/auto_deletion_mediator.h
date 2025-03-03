// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/ui/auto_deletion/auto_deletion_mutator.h"

class Browser;
class PrefService;
namespace web {
class DownloadTask;
}  // namespace web

// Mediator for the Auto-deletion UI.
@interface AutoDeletionMediator : NSObject <AutoDeletionMutator>

- (instancetype)initWithLocalState:(PrefService*)localState
                           browser:(Browser*)browser
                      downloadTask:(web::DownloadTask*)task
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AUTO_DELETION_AUTO_DELETION_MEDIATOR_H_
