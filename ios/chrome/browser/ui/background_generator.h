// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BACKGROUND_GENERATOR_H_
#define IOS_CHROME_BROWSER_UI_BACKGROUND_GENERATOR_H_

#import <UIKit/UIKit.h>

// Returns a UIImage of size |backgroundRect| with a radial gradient image at
// |centerPoint| and radiates outwards to a radius of |radius|. The gradient
// starts from |centerColor| at |centerPoint| to |outsideColor| at the end of
// |radius|. |tileImage| is tiled over the entire image and |logoImage| is
// rendered at |centerPoint|.
// |tileImage| and |logoImage| may be nil.
UIImage* GetRadialGradient(CGRect backgroundRect,
                           CGPoint centerPoint,
                           CGFloat radius,
                           CGFloat centerColor,
                           CGFloat outsideColor,
                           UIImage* tileImage,
                           UIImage* logoImage);

#endif  // IOS_CHROME_BROWSER_UI_BACKGROUND_GENERATOR_H_
