// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_AUDIENCE_H_

namespace segmentation_platform {
enum class TipIdentifier;
}  // namespace segmentation_platform

// Interface to handle Tips card user events.
@protocol TipsModuleAudience

// Called when a Tips item matching `identifier` is selected by the user.
- (void)didSelectTip:(segmentation_platform::TipIdentifier)identifier;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_TIPS_TIPS_MODULE_AUDIENCE_H_
