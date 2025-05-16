// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

// A reusable Header view for the background preset section in the collection
// view.
@interface HomeCustomizationBackgroundPresetHeaderView
    : UICollectionReusableView

// Sets the text displayed in the header label.
- (void)setText:(NSString*)text;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_BACKGROUND_PRESET_HEADER_VIEW_H_
