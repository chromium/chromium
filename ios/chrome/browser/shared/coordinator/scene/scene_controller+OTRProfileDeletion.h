// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_OTRPROFILEDELETION_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_OTRPROFILEDELETION_H_

#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"

@interface SceneController (OTRProfileDeletion)

// Must be called before destroying the incognito Profile.
- (void)willDestroyIncognitoProfile;

// Must be called after recreating the incognito Profile.
- (void)incognitoProfileCreated;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_CONTROLLER_OTRPROFILEDELETION_H_
