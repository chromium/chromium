// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_FAKES_FAKE_WEB_CONTENT_HANDLER_H_
#define IOS_CHROME_TEST_FAKES_FAKE_WEB_CONTENT_HANDLER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/public/commands/web_content_commands.h"

// test fake for WebContentCommand callers. Records arguments passed into the
// command methods for tests to examine.
@interface FakeWebContentHandler : NSObject <WebContentCommands>

// The most recent dictionary passed into -showAppStoreWithParameters:.
@property(nonatomic) NSDictionary* productParams;

// All passes passed into -showDialogForPassKitPasses. nil passes are
// represented with NSNull objects.
@property(nonatomic, readonly) NSArray* passes;

// Whether any method on this fake object has been called.
@property(nonatomic, assign) BOOL called;

@end

#endif  // IOS_CHROME_TEST_FAKES_FAKE_WEB_CONTENT_HANDLER_H_
