// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_ALPHA_ANIMATED_COLLECTION_VIEW_FLOW_LAYOUT_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_ALPHA_ANIMATED_COLLECTION_VIEW_FLOW_LAYOUT_H_

#import "ios/third_party/material_components_ios/src/components/Collections/src/MDCCollectionViewFlowLayout.h"

// An |MDCCollectionViewFlowLayout| that uses alpha for animation, which makes
// rotation on the device look smoother.
@interface AlphaAnimatedCollectionViewFlowLayout : MDCCollectionViewFlowLayout
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_ALPHA_ANIMATED_COLLECTION_VIEW_FLOW_LAYOUT_H_
