// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/favicon/favicon_loader.h"

// Delegate for the passwords bottom sheet.
@protocol PasswordSuggestionBottomSheetDelegate

// Sends the information about which suggestion from the bottom sheet was
// selected by the user, which is expected to fill the relevant fields.
- (void)didSelectSuggestion:(NSInteger)row;

// Request to refocus the field which originally triggered the bottom sheet
// after the bottom sheet has been dismissed.
- (void)refocus;

// Disables future refocus requests.
- (void)disableRefocus;

// Loads the favicon for cell. Defaults to the globe symbol if the URL is empty.
- (void)loadFaviconWithBlockHandler:
    (FaviconLoader::FaviconAttributesCompletionBlock)faviconLoadedBlock;
@end

#endif  // IOS_CHROME_BROWSER_UI_PASSWORDS_BOTTOM_SHEET_PASSWORD_SUGGESTION_BOTTOM_SHEET_DELEGATE_H_
