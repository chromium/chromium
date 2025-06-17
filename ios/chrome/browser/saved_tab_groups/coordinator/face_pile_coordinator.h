// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_COORDINATOR_FACE_PILE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_COORDINATOR_FACE_PILE_COORDINATOR_H_

#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_providing.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class FacePileConfiguration;

// Coordinator for the FacePile feature.
// This coordinator is responsible for creating and managing a FacePileView
// and its associated FacePileMediator.
// It is the responsibility of the caller to add the face pile to the view
// hierarchy. On the other hand, when this coordinator is stopped, the view is
// automatically removed from the view hierarchy.
@interface FacePileCoordinator : ChromeCoordinator <FacePileProviding>

- (instancetype)initWithFacePileConfiguration:
                    (FacePileConfiguration*)configuration
                                      browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_COORDINATOR_FACE_PILE_COORDINATOR_H_
