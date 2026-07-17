// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_

#import <UIKit/UIKit.h>

@class ImageContentConfiguration;

namespace autofill {

// Returns the symbol configuration to use for the close button.
UIImageSymbolConfiguration* GetCloseButtonSymbolConfiguration();

// Returns the foreground color to use for the close button color palette.
UIColor* GetCloseButtonForegroundColor();

// Creates and returns an ImageContentConfiguration, following the design system
// for AtMemory cell icons.
ImageContentConfiguration* AtMemoryCellIconConfiguration(NSString* symbol_name);

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
