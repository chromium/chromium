// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_TEST_SESSION_SERVICE_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_TEST_SESSION_SERVICE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/sessions/model/session_service_ios.h"

// Testing subclass of SessionService that immediately consumes session windows
// passed to -saveSessionWindow:sessionPath:immediately: is consumed immediately
// but only saved to disk if `performIO` is set to YES. Also it keeps track of
// how many calls to saveSessionWindow have been done.
@interface TestSessionService : SessionServiceIOS

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithSaveDelay:(base::TimeDelta)saveDelay
                       taskRunner:
                           (const scoped_refptr<base::SequencedTaskRunner>&)
                               taskRunner NS_UNAVAILABLE;

// If YES, then sessions are saved to disk, otherwise, data is discarded.
@property(nonatomic, assign) BOOL performIO;

@property(nonatomic, readonly) int saveSessionCallsCount;

@property(nonatomic, readonly) int loadSessionCallsCount;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_TEST_SESSION_SERVICE_H_
