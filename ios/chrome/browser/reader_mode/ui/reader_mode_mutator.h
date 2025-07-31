// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_MUTATOR_H_
#define IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_MUTATOR_H_

#import <UIKit/UIKit.h>

namespace dom_distiller::mojom {
enum class Theme;
}  // namespace dom_distiller::mojom

// Mutator for the Reader Mode.
@protocol ReaderModeMutator

// Sets the default theme for the distilled page.
- (void)setDefaultTheme:(dom_distiller::mojom::Theme)theme;

@end

#endif  // IOS_CHROME_BROWSER_READER_MODE_UI_READER_MODE_MUTATOR_H_
