// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_RING_IMAGE_VIEW_H_
#define IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_RING_IMAGE_VIEW_H_

#import <UIKit/UIKit.h>

// A UIImageView subclass that uses a CAShapeLayer mask to display only the
// outer ring of its image.
// It must be initialized with a disk. The size of the ring is
// `kAiTierRingWidth`.
@interface AITierRingImageView : UIImageView

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_UI_AVATAR_AI_TIER_RING_IMAGE_VIEW_H_
