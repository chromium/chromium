// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_MUTATOR_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_MUTATOR_H_

/// Mutator for the result page.
@protocol LensResultPageMutator

/// Sets the user interface style mode.
- (void)setIsDarkMode:(BOOL)isDarkMode;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_RESULT_PAGE_MUTATOR_H_
