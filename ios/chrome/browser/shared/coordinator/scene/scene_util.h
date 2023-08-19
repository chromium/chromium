// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UTIL_H_

#import <Foundation/Foundation.h>

@class UIScene;

// Returns the identifier to use for the session for `scene`.
NSString* SessionIdentifierForScene(UIScene* scene);

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UTIL_H_
