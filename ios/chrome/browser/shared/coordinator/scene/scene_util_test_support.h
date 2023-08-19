// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UTIL_TEST_SUPPORT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UTIL_TEST_SUPPORT_H_

#import <UIKit/UIKit.h>

// Returns a fake UIScene with `identifier` as session persistent identifier
// when running on iOS 13+ or nil otherwise. The fake object implements just
// enough API for SessionIdentifierForScene().
id FakeSceneWithIdentifier(NSString* identifier);

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_UTIL_TEST_SUPPORT_H_
