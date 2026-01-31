// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_DELEGATE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_DELEGATE_H_

#import <Foundation/Foundation.h>

@class FaviconAttributes;

// Delegate for the credential bottom sheet.
@protocol CredentialSuggestionBottomSheetDelegate

// Request to disable the bottom sheet, potentially refocusing the field which
// originally triggered the bottom sheet after the bottom sheet has been
// disabled.
- (void)disableBottomSheet;

// Loads the favicon for cell. Defaults to the globe symbol if the URL is empty.
- (void)loadFaviconWithBlockHandler:(void (^)(FaviconAttributes* attributes,
                                              bool cached))faviconLoadedBlock;
@end

#endif  // IOS_CHROME_BROWSER_PASSWORDS_BOTTOM_SHEET_UI_CREDENTIAL_SUGGESTION_BOTTOM_SHEET_DELEGATE_H_
