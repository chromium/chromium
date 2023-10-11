// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DELEGATE_H_

// Protocol for relaying events from the Content Suggestions to the NTP.
@protocol ContentSuggestionsDelegate

// Informs the delegate that the ContentSuggestionsViewController has been
// updated.
- (void)contentSuggestionsWasUpdated;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_DELEGATE_H_
