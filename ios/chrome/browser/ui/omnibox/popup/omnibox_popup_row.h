// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_ROW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_ROW_H_

#import <UIKit/UIKit.h>

@class OmniboxPopupTruncatingLabel;

// View used to display an omnibox autocomplete match in the omnibox popup.
@interface OmniboxPopupRow : UITableViewCell

// A truncate-by-fading version of the textLabel of a UITableViewCell.
@property(nonatomic, readonly, strong)
    OmniboxPopupTruncatingLabel* textTruncatingLabel;
// A truncate-by-fading version of the detailTextLabel of a UITableViewCell.
@property(nonatomic, readonly, strong)
    OmniboxPopupTruncatingLabel* detailTruncatingLabel;
// A standard UILabel for answers, which truncates with ellipses to support
// multi-line text.
@property(nonatomic, readonly, strong) UILabel* detailAnswerLabel;

@property(nonatomic, readonly, strong) UIImageView* imageView;
@property(nonatomic, readonly, strong) UIImageView* answerImageView;
@property(nonatomic, readonly, strong) UIButton* trailingButton;
@property(nonatomic, assign) CGFloat rowHeight;
// Whether this row is displaying a TabMatch. If YES, the trailing icon is
// updated to reflect that.
@property(nonatomic, assign, getter=isTabMatch) BOOL tabMatch;

// Initialize the row with the given incognito state. The colors and styling are
// dependent on whether or not the row is displayed in incognito mode.
- (instancetype)initWithIncognito:(BOOL)incognito;

// Update the match type icon with the supplied image ID and adjust its position
// based on the current size of the row.
- (void)updateLeadingImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_POPUP_ROW_H_
