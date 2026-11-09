// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_TESTING_H_
#define IOS_CHROME_APP_TASK_REQUEST_TESTING_H_

#import "ios/chrome/app/task_request.h"

@class UIScene;

@interface TaskRequest (Testing)

@property(nonatomic, strong, readwrite) NSString* gaiaID;

- (instancetype)initWithScene:(UIScene*)scene;

+ (instancetype)taskForTestingWithScene:(UIScene*)scene
                           executeBlock:(ProceduralBlock)block;

// Returns a task for `scene` associated with `gaiaID` and `URL`, whose
// execution does nothing.
+ (instancetype)taskForTestingWithScene:(UIScene*)scene
                                 gaiaID:(NSString*)gaiaID
                                    URL:(NSURL*)URL;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_TESTING_H_
