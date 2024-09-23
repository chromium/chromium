// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_BROWSER_INTEREST_SIGNALS_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_BROWSER_INTEREST_SIGNALS_H_

@class SceneState;

namespace feature_engagement {
class Tracker;
}

namespace default_browser {

// Records all necessary information for Chrome start with widget.
void NotifyStartWithWidget(feature_engagement::Tracker* tracker);

// Records all necessary information for Chrome start with URL event.
void NotifyStartWithURL(feature_engagement::Tracker* tracker);

// Records all necessary information for Credential Extension use.
void NotifyCredentialExtensionUsed(feature_engagement::Tracker* tracker);

// Records all necessary information when autofill suggestions were shown to the
// user. Except passwords.
void NotifyAutofillSuggestionsShown(feature_engagement::Tracker* tracker);

// Records all necessary information when password suggestion is used.
void NotifyPasswordAutofillSuggestionUsed(feature_engagement::Tracker* tracker);

// Records all necessary information when password is saved or updated through
// infobar.
void NotifyPasswordSavedOrUpdated(feature_engagement::Tracker* tracker);

// Records all necessary information when remote tabs grid is selected.
void NotifyRemoteTabsGridViewed(feature_engagement::Tracker* tracker);

// Records all necessary information when user added or edited a bookmark.
void NotifyBookmarkAddOrEdit(feature_engagement::Tracker* tracker);

// Records all necessary information when user opens bookmark manager.
void NotifyBookmarkManagerOpened(feature_engagement::Tracker* tracker);

// Records all necessary information when user closes bookmark manager.
// TODO(b/315330160): Consider not tracking Bookmark manager close events.
void NotifyBookmarkManagerClosed(feature_engagement::Tracker* tracker);

// Records all necessary information when user opens a URL from bookmarks.
void NotifyURLFromBookmarkOpened(feature_engagement::Tracker* tracker);

// Records all necessary information when user copy-pastes a URL in omnibox.
void NotifyOmniboxURLCopyPaste(feature_engagement::Tracker* tracker);

// Records all necessary information when user copy-pastes and navigates to a
// URL in omnibox.
void NotifyOmniboxURLCopyPasteAndNavigate(bool is_off_record,
                                          feature_engagement::Tracker* tracker,
                                          SceneState* scene_state);

// Records all necessary information when user copy-pastes and searches a text
// in omnibox.
void NotifyOmniboxTextCopyPasteAndNavigate(
    feature_engagement::Tracker* tracker);

// Record all necessary information when Default Browser FRE promo is shown.
void NotifyDefaultBrowserFREPromoShown(feature_engagement::Tracker* tracker);

}  // namespace default_browser

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_MODEL_DEFAULT_BROWSER_INTEREST_SIGNALS_H_
