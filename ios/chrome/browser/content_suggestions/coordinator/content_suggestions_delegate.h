// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_COORDINATOR_CONTENT_SUGGESTIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_COORDINATOR_CONTENT_SUGGESTIONS_DELEGATE_H_

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

// Opens the Home Customization menu at the Main page, optionally presenting an
// IPH bubble on the customization button when dismissed if
// `withIconBubbleOnDismiss` is YES, and scrolling to the bottom of the sheet if
// `scrollToBottom` is YES.
- (void)openMainCustomizationMenuWithIconBubbleOnDismiss:
            (BOOL)withIconBubbleOnDismiss
                                          scrollToBottom:(BOOL)scrollToBottom;

// Opens the Safari Data import instructions.
- (void)openSafariDataImport;

// Opens the photo picker for selecting a custom NTP background from the
// device's photo library.
- (void)openNTPBackgroundPhotoPicker;

// Presents the IPH bubble pointing to the Home customization menu button.
- (void)presentCustomizationButtonBubble;

// Returns `YES` if the NTP is bypassed because the omnibox is focused or
// scheduled to be shown, or because a post-opening startup action is pending
// (for example, when selecting "Search" from the app icon long-press menu).
- (BOOL)isNTPBypassed;

// Triggers the radial wipe animation to hide the Magic Stack and Discover feed,
// disables both modules, and shows an undo snackbar.
- (void)applyFocusedNTPWithUndoSnackbar;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_COORDINATOR_CONTENT_SUGGESTIONS_DELEGATE_H_
