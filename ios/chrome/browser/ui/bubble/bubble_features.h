// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_FEATURES_H_

#include "base/feature_list.h"

// Feature flag to enable rich IPH on bubbles.
BASE_DECLARE_FEATURE(kBubbleRichIPH);

// Feature parameters for rich IPH on bubbles. If no parameter is set, the
// default bubble style will be used.
extern const char kBubbleRichIPHParameterName[];

// Default bubble view.
extern const char kBubbleRichIPHParameterTargetHighlight[];
// Wide bubble view with explicit dismissal.
extern const char kBubbleRichIPHParameterExplicitDismissal[];
// Wide bubble view with explicit dismissal and rich content (title, image,
// description).
extern const char kBubbleRichIPHParameterRich[];
// Wide bubble view with explicit dismissal, rich content and snooze button.
extern const char kBubbleRichIPHParameterRichWithSnooze[];

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_FEATURES_H_
