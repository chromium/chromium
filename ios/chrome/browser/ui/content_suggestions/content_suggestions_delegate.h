// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DELEGATE_H_

class GURL;

// Protocol for relaying events from the Content Suggestions to the NTP.
@protocol ContentSuggestionsDelegate

// Informs the delegate that the ContentSuggestionsViewController has been
// updated.
- (void)contentSuggestionsWasUpdated;

// Triggers the URL sharing flow for the given `URL` and `title`, with the
// origin `view` representing the UI component for that URL.
- (void)shareURL:(const GURL&)URL title:(NSString*)title fromView:(UIView*)view;

// Opens the Home Customization menu at the Magic Stack page.
- (void)openMagicStackCustomizationMenu;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DELEGATE_H_
