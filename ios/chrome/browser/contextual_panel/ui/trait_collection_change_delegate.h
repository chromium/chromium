// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_TRAIT_COLLECTION_CHANGE_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_TRAIT_COLLECTION_CHANGE_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol TraitCollectionChangeDelegate

- (void)traitCollectionDidChangeForViewController:
    (UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_TRAIT_COLLECTION_CHANGE_DELEGATE_H_
