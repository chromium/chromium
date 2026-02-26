// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_TESTING_H_
#define IOS_CHROME_APP_TASK_REQUEST_TESTING_H_

#import "ios/chrome/app/task_request.h"

@interface TaskRequest (Testing)

@property(nonatomic, strong, readwrite) NSString* gaiaID;

- (instancetype)initWithSceneID:(std::string_view)sceneID;

+ (instancetype)taskForTestingWithSceneID:(std::string_view)sceneID
                             executeBlock:(ProceduralBlock)block;

@end

#endif  // IOS_CHROME_APP_TASK_REQUEST_TESTING_H_
