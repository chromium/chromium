// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PageInfoPresentation;

// The coordinator for the page info.
@interface PageInfoCoordinator : ChromeCoordinator

// `presentationProvider` provides information and runs tasks necessary to
// present Page Info.
@property(nonatomic, weak) id<PageInfoPresentation> presentationProvider;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_COORDINATOR_H_
