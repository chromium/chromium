// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AR_QUICK_LOOK_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AR_QUICK_LOOK_MEDIATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate protocol for ARQuickLookMediator.
@protocol ARQuickLookMediatorDelegate <NSObject>

// Requests the dismissal of the USDZ preview.
- (void)dismissUSDZPreview;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_COORDINATOR_AR_QUICK_LOOK_MEDIATOR_DELEGATE_H_
