// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_MATCH_PREVIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_MATCH_PREVIEW_DELEGATE_H_

// Receives match previews for display.
// Used by the popup to inform the omnibox textfield about the currently
// highlighted suggestion; the textfield shows the suggestion text and image.
@protocol PopupMatchPreviewDelegate

- (void)setPreviewMatchText:(NSAttributedString*)text image:(id)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_MATCH_PREVIEW_DELEGATE_H_
