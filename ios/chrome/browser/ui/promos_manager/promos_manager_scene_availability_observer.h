// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_SCENE_AVAILABILITY_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_SCENE_AVAILABILITY_OBSERVER_H_

#import <UIKit/UIKit.h>

// Observer protocol to inform about scene availability changes. Informs
// observers when the scene is deemed to be in a valid, eligible state
// to display a promo.
@protocol PromosManagerSceneAvailabilityObserver <NSObject>

// Indicates the scene has changed and become available to display a
// promo.
- (void)sceneDidBecomeAvailableForPromo;

@end

#endif  // IOS_CHROME_BROWSER_UI_PROMOS_MANAGER_PROMOS_MANAGER_SCENE_AVAILABILITY_OBSERVER_H_
