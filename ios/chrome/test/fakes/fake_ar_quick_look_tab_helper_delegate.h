// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_AR_QUICK_LOOK_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_TEST_FAKES_FAKE_AR_QUICK_LOOK_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/download/model/ar_quick_look_tab_helper_delegate.h"

// ARQuickLookTabHelperDelegate which collects all file URLs into `fileURLs`.
@interface FakeARQuickLookTabHelperDelegate
    : NSObject <ARQuickLookTabHelperDelegate>

// All file URLs downloaded and presented by ARQuickLookTabHelper. These URLs
// cannot be nil.
@property(nonatomic, readonly) NSArray* fileURLs;

// The value of `canonicalWebPageURL` for the most recent file URL downloaded
// by ARQuickLookTabHelper.
@property(nonatomic, readonly) NSURL* canonicalWebPageURL;

// The value of `allowsContentScaling` for the most recent file URL downloaded
// by ARQuickLookTabHelper.
@property(nonatomic, readonly) BOOL allowsContentScaling;

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_AR_QUICK_LOOK_TAB_HELPER_DELEGATE_H_
