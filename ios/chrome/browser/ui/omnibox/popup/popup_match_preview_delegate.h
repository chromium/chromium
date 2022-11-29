// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_MATCH_PREVIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_MATCH_PREVIEW_DELEGATE_H_

@protocol AutocompleteSuggestion;

/// Receives match previews for display.
/// Used by the popup to inform the omnibox textfield about the currently
/// highlighted suggestion; the textfield shows the suggestion text and image.
@protocol PopupMatchPreviewDelegate

/// Notifies the delegate of the suggestion to preview.
/// `suggestion` can be nil e.g. when there is no highlighting in the popup.
/// `isFirstUpdate` flag is set when this is the first suggestion for a new set
/// of results. This may be used to display the suggestion in a different way,
/// e.g. as inline autocomplete.
- (void)setPreviewSuggestion:(id<AutocompleteSuggestion>)suggestion
               isFirstUpdate:(BOOL)isFirstUpdate;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_POPUP_MATCH_PREVIEW_DELEGATE_H_
