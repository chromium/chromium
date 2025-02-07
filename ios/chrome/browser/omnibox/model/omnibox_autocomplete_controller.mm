// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"

#import <string>

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/omnibox/model/omnibox_popup_controller.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_view_ios.h"
#import "third_party/omnibox_proto/groups.pb.h"

using base::UserMetricsAction;

@implementation OmniboxAutocompleteController {
  /// Controller of the omnibox.
  raw_ptr<OmniboxController> _omniboxController;
  /// Controller of autocomplete.
  raw_ptr<AutocompleteController> _autocompleteController;
  /// Controller of the omnibox view.
  raw_ptr<OmniboxViewIOS> _omniboxViewIOS;
}

- (instancetype)initWithOmniboxController:(OmniboxController*)omniboxController
                           omniboxViewIOS:(OmniboxViewIOS*)omniboxViewIOS {
  self = [super init];
  if (self) {
    _omniboxController = omniboxController;
    _autocompleteController = omniboxController->autocomplete_controller();
    _omniboxViewIOS = omniboxViewIOS;
  }
  return self;
}

- (void)disconnect {
  _autocompleteController = nullptr;
  _omniboxController = nullptr;
  _omniboxViewIOS = nullptr;
}

#pragma mark - OmniboxEditModel event

- (void)updatePopupSuggestions {
  if (_autocompleteController) {
    BOOL isFocusing = _autocompleteController->input().focus_type() ==
                      metrics::OmniboxFocusType::INTERACTION_FOCUS;
    [self.omniboxPopupController
        newResultsAvailable:_autocompleteController->result()
                  isOnFocus:isFocusing];
  }
}

#pragma mark - OmniboxPopup event

- (void)requestResultsWithVisibleSuggestionCount:
    (NSUInteger)visibleSuggestionCount {
  if (!_autocompleteController) {
    return;
  }
  size_t resultSize = _autocompleteController->result().size();
  // If no suggestions are visible, consider all of them visible.
  if (visibleSuggestionCount == 0) {
    visibleSuggestionCount = resultSize;
  }
  NSUInteger visibleSuggestions = MIN(visibleSuggestionCount, resultSize);
  if (visibleSuggestions > 0) {
    // Groups visible suggestions by search vs url. Skip the first suggestion
    // because it's the omnibox content.
    _autocompleteController->GroupSuggestionsBySearchVsURL(1,
                                                           visibleSuggestions);
  }
  // Groups hidden suggestions by search vs url.
  if (visibleSuggestions < resultSize) {
    _autocompleteController->GroupSuggestionsBySearchVsURL(visibleSuggestions,
                                                           resultSize);
  }

  [self.omniboxPopupController
      updateWithSortedResults:_autocompleteController->result()];
}

- (BOOL)isStarredMatch:(const AutocompleteMatch&)match {
  if (_omniboxController && _omniboxController->client()) {
    auto* bookmark_model = _omniboxController->client()->GetBookmarkModel();
    return bookmark_model &&
           bookmark_model->IsBookmarked(match.destination_url);
  }
  return NO;
}

- (void)selectMatchForOpening:(const AutocompleteMatch&)match
                        inRow:(NSUInteger)row
                       openIn:(WindowOpenDisposition)disposition {
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, `match` and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch matchCopy = match;

  if (matchCopy.type == AutocompleteMatchType::CLIPBOARD_URL) {
    // TODO(crbug.com/326989399): MobileOmniboxClipboardToURL action is not
    // defined in actions.xml
    base::RecordAction(UserMetricsAction("MobileOmniboxClipboardToURL"));
    UMA_HISTOGRAM_LONG_TIMES_100(
        "MobileOmnibox.PressedClipboardSuggestionAge",
        ClipboardRecentContent::GetInstance()->GetClipboardContentAge());
  }
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnSelectedMatchForOpening(matchCopy, disposition, GURL(),
                                               std::u16string(), row);
  }
}

- (void)selectMatchForAppending:(const AutocompleteMatch&)match {
  // Make a defensive copy of `match.fill_into_edit`, as CopyToOmnibox() will
  // trigger a new round of autocomplete and modify `match`.
  std::u16string fill_into_edit(match.fill_into_edit);

  // If the match is not a URL, append a whitespace to the end of it.
  if (AutocompleteMatch::IsSearchType(match.type)) {
    fill_into_edit.append(1, ' ');
  }

  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnSelectedMatchForAppending(fill_into_edit);
  }
}

- (void)selectMatchForDeletion:(const AutocompleteMatch&)match {
  if (_autocompleteController) {
    _autocompleteController->DeleteMatch(match);
  }
}

- (void)onScroll {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnPopupDidScroll();
  }
}

- (void)onCallAction {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnCallActionTap();
  }
}

@end
