// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/bubble/bubble_features.h"

const base::Feature kBubbleRichIPH{"BubbleRichIPH",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

const char kBubbleRichIPHParameterName[] = "BubbleVariant";

const char kBubbleRichIPHParameterTargetHighlight[] = "TargetHighlight";
const char kBubbleRichIPHParameterExplicitDismissal[] = "ExplicitDismissal";
const char kBubbleRichIPHParameterRich[] = "RichContentWithDismissal";
const char kBubbleRichIPHParameterRichWithSnooze[] =
    "RichContentWithDismissalAndSnooze";
