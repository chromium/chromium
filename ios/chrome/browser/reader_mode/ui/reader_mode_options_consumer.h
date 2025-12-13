// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_CONSUMER_H_

#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"

// A protocol for a consumer of Reader mode options.
@protocol ReaderModeOptionsConsumer <NSObject>

// Sets `fontFamily` as the currently selected font family.
- (void)setSelectedFontFamily:(dom_distiller::mojom::FontFamily)fontFamily;

// Sets `theme` as the currently selected theme.
- (void)setSelectedTheme:(dom_distiller::mojom::Theme)theme;

// Sets the decrease font size button status to `enabled`.
- (void)setDecreaseFontSizeButtonEnabled:(BOOL)enabled;

// Sets the increase font size button status to `enabled`.
- (void)setIncreaseFontSizeButtonEnabled:(BOOL)enabled;

// Announces the given `multiplier` for the font size.
- (void)announceFontSizeMultiplier:(CGFloat)multiplier;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_CONSUMER_H_
