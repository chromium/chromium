// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_MUTATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_MUTATOR_H_

#import <Foundation/Foundation.h>

#import "components/dom_distiller/core/mojom/distilled_page_prefs.mojom.h"

// Mutator protocol for Reader mode options.
@protocol ReaderModeOptionsMutator <NSObject>

// Sets the font family for Reader mode.
- (void)setFontFamily:(dom_distiller::mojom::FontFamily)fontFamily;

// Increases the font size.
- (void)increaseFontSize;

// Decreases the font size.
- (void)decreaseFontSize;

// Sets the theme for the Reader mode page.
- (void)setTheme:(dom_distiller::mojom::Theme)theme;

// Hides Reader mode.
- (void)hideReaderMode;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_OPTIONS_MUTATOR_H_
