// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_GESTURES_VIEW_CONTROLLER_TRAIT_COLLECTION_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_GESTURES_VIEW_CONTROLLER_TRAIT_COLLECTION_OBSERVER_H_

// Protocol for an object observing changes in the traitCollection.
@protocol ViewControllerTraitCollectionObserver <NSObject>

- (void)viewController:(UIViewController*)viewController
    traitCollectionDidChange:(UITraitCollection*)previousTraitCollection;

@end

#endif  // IOS_CHROME_BROWSER_UI_GESTURES_VIEW_CONTROLLER_TRAIT_COLLECTION_OBSERVER_H_
