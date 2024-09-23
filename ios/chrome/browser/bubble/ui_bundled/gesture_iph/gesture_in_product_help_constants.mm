// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/gesture_iph/gesture_in_product_help_constants.h"

#import "base/time/time.h"

NSString* const kGestureInProductHelpViewBackgroundAXId =
    @"GestureInProductHelpViewBackgroundAXId";

NSString* const kGestureInProductHelpViewBubbleAXId =
    @"GestureInProductHelpViewBubbleAXId";

NSString* const kGestureInProductHelpViewDismissButtonAXId =
    @"GestureInProductHelpViewDismissButtonAXId";

base::TimeDelta const kGestureInProductHelpViewAppearDuration =
    base::Milliseconds(250);

base::TimeDelta const kDurationBetweenBidirectionalCycles =
    base::Milliseconds(250);

CGFloat const kGestureIndicatorRadius = 33.0f;
