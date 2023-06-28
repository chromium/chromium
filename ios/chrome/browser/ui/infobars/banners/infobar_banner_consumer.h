// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol InfobarBannerConsumer <NSObject>

// Accessibility label for the banner view.  Default value is the concatenation
// of the title and subtitle texts.
- (void)setBannerAccessibilityLabel:(NSString*)bannerAccessibilityLabel;

// The button text displayed by this InfobarBanner.
- (void)setButtonText:(NSString*)buttonText;

// The favicon displayed by this InfobarBanner.
- (void)setFaviconImage:(UIImage*)faviconImage;

// The icon displayed by this InfobarBanner.
- (void)setIconImage:(UIImage*)iconImage;

// The tint color of the icon image.
- (void)setIconImageTintColor:(UIColor*)iconImageTintColor;

// YES if the icon image should have a default tint applied to its background.
- (void)setUseIconBackgroundTint:(BOOL)useIconBackgroundTint;

// NO if the icon image colors should not be ignored when a background tint is
// applied. Default is YES.
- (void)setIgnoreIconColorWithTint:(BOOL)ignoreIconColorWithTint;

// The background color of the icon, only applied when
// [setUseIconBackgroundTint:YES] is called.
- (void)setIconBackgroundColor:(UIColor*)iconBackgroundColor;

// YES if the banner should be able to present a Modal. Changing this property
// will immediately update the Banner UI that is related to triggering modal
// presentation.
- (void)setPresentsModal:(BOOL)presentsModal;

// The title displayed by this InfobarBanner.
- (void)setTitleText:(NSString*)titleText;

// The subtitle displayed by this InfobarBanner.
- (void)setSubtitleText:(NSString*)subtitleText;

// Sets the number of maximum lines in title. Default value is 0 (no maximum
// limit).
- (void)setTitleNumberOfLines:(NSInteger)titleNumberOfLines;

// Sets the number of maximum lines in subtitle. Default value is 0 (no maximum
// limit).
- (void)setSubtitleNumberOfLines:(NSInteger)subtitleNumberOfLines;

// Sets the lineBreakMode of the subtitle text. Default value is
// NSLineBreakByTruncatingTail.
- (void)setSubtitleLineBreakMode:(NSLineBreakMode)linebreakMode;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_CONSUMER_H_
