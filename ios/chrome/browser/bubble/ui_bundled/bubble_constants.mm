// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"

// Accessibility identifier for the close button.
NSString* const kBubbleViewCloseButtonIdentifier =
    @"BubbleViewCloseButtonIdentifier";
// Accessibility identifier for the title label.
NSString* const kBubbleViewTitleLabelIdentifier =
    @"BubbleViewTitleLabelIdentifier";
// Accessibility identifier for the label.
NSString* const kBubbleViewLabelIdentifier = @"BubbleViewLabelIdentifier";
// Accessibility identifier for the image view.
NSString* const kBubbleViewImageViewIdentifier =
    @"BubbleViewImageViewIdentifier";
// Accessibility identifier for the snooze button.
NSString* const kBubbleViewSnoozeButtonIdentifier =
    @"kBubbleViewSnoozeButtonIdentifier";
// Accessibility identifier for the arrow view.
NSString* const kBubbleViewArrowViewIdentifier =
    @"kBubbleViewArrowViewIdentifier";
// How long, in seconds, the bubble is visible on the screen.
NSTimeInterval const kBubbleVisibilityDuration = 5.0;
// How long, in seconds, the default "long duration" bubbles are visible.
NSTimeInterval const kDefaultLongDurationBubbleVisibility = 8.0;
// Metric name for bubble dismissal tracking.
const char kUMAIPHDismissalReason[] = "InProductHelp.DismissalReason.iOS";
// Metric name for gestural bubble dismissal tracking.
const char kUMAGesturalIPHDismissalReason[] =
    "InProductHelp.Gestural.DismissalReason.iOS";
