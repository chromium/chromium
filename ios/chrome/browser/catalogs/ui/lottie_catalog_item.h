// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CATALOGS_UI_LOTTIE_CATALOG_ITEM_H_
#define IOS_CHROME_BROWSER_CATALOGS_UI_LOTTIE_CATALOG_ITEM_H_

#import <UIKit/UIKit.h>

// Entry of the Lottie catalog. Holds the name of a bundled Lottie animation and
// the visual configuration (colors) mirroring how its production presenter
// displays it.
@interface LottieCatalogItem : NSObject

// Name of the animation file, without the `.json` extension.
@property(nonatomic, copy) NSString* animationName;

// Name of the feature displaying the animation.
@property(nonatomic, copy) NSString* feature;

// Colors applied to the animation, keyed by Lottie keypath, in light and dark
// mode. When nil, the animation's embedded colors are used.
@property(nonatomic, copy)
    NSDictionary<NSString*, UIColor*>* lightModeColorProvider;
@property(nonatomic, copy)
    NSDictionary<NSString*, UIColor*>* darkModeColorProvider;

// Background color behind the animation.
@property(nonatomic, strong) UIColor* animationBackgroundColor;

@end

#endif  // IOS_CHROME_BROWSER_CATALOGS_UI_LOTTIE_CATALOG_ITEM_H_
