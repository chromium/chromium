// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"

#import <QuartzCore/QuartzCore.h>

#include <memory>
#include <string>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_suggestions_delegate.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

OmniboxPopupViewIOS::OmniboxPopupViewIOS(
    OmniboxEditModel* edit_model,
    OmniboxPopupViewSuggestionsDelegate* delegate)
    : edit_model_(edit_model), delegate_(delegate) {
  DCHECK(delegate);
  DCHECK(edit_model);
  edit_model->set_popup_view(this);
}

OmniboxPopupViewIOS::~OmniboxPopupViewIOS() {
  edit_model_->set_popup_view(nullptr);
}

// Set left image to globe or magnifying glass depending on which autocomplete
// option is highlighted.
void OmniboxPopupViewIOS::UpdateEditViewIcon() {
  const AutocompleteResult& result = model()->result();

  // Use default icon as a fallback
  if (model()->GetPopupSelection().line == OmniboxPopupSelection::kNoMatch) {
    delegate_->OnSelectedMatchImageChanged(/*has_match=*/false,
                                           AutocompleteMatchType::NUM_TYPES,
                                           absl::nullopt, GURL());
    return;
  }

  const AutocompleteMatch& match =
      result.match_at(model()->GetPopupSelection().line);

  absl::optional<SuggestionAnswer::AnswerType> optAnswerType = absl::nullopt;
  if (match.answer && match.answer->type() > 0 &&
      match.answer->type() <
          SuggestionAnswer::AnswerType::ANSWER_TYPE_TOTAL_COUNT) {
    optAnswerType =
        static_cast<SuggestionAnswer::AnswerType>(match.answer->type());
  }
  delegate_->OnSelectedMatchImageChanged(/*has_match=*/true, match.type,
                                         optAnswerType, match.destination_url);
}

void OmniboxPopupViewIOS::UpdatePopupAppearance() {
  const AutocompleteResult& result = model()->result();

  [mediator_ updateWithResults:result];
  if ([mediator_ isOpen]) {
    UpdateEditViewIcon();
  }

  delegate_->OnResultsChanged(result);
}

bool OmniboxPopupViewIOS::IsOpen() const {
  return [mediator_ hasResults];
}

OmniboxEditModel* OmniboxPopupViewIOS::model() const {
  return edit_model_;
}

#pragma mark - OmniboxPopupProvider

bool OmniboxPopupViewIOS::IsPopupOpen() {
  return [mediator_ isOpen];
}

void OmniboxPopupViewIOS::SetTextAlignment(NSTextAlignment alignment) {
  [mediator_ setTextAlignment:alignment];
}

void OmniboxPopupViewIOS::SetSemanticContentAttribute(
    UISemanticContentAttribute semanticContentAttribute) {
  [mediator_ setSemanticContentAttribute:semanticContentAttribute];
}

#pragma mark - OmniboxPopupViewControllerDelegate

bool OmniboxPopupViewIOS::IsStarredMatch(const AutocompleteMatch& match) const {
  return edit_model_->IsStarredMatch(match);
}

void OmniboxPopupViewIOS::OnHighlightCanceled() {
  model()->SetPopupSelection(
      OmniboxPopupSelection(OmniboxPopupSelection::kNoMatch), true, true);
}

void OmniboxPopupViewIOS::OnMatchHighlighted(size_t row) {
  model()->SetPopupSelection(OmniboxPopupSelection(row), false, true);
  if ([mediator_ isOpen]) {
    UpdateEditViewIcon();
  }
}

void OmniboxPopupViewIOS::OnMatchSelected(
    const AutocompleteMatch& selectedMatch,
    size_t row,
    WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = selectedMatch;

  if (match.type == AutocompleteMatchType::CLIPBOARD_URL ||
      match.type == AutocompleteMatchType::CLIPBOARD_TEXT) {
    // A search using clipboard link or text is activity that should indicate a
    // user that would be interested in setting Chrome as the default browser.
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  }

  if (match.type == AutocompleteMatchType::CLIPBOARD_URL) {
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
    base::RecordAction(UserMetricsAction("MobileOmniboxClipboardToURL"));
    UMA_HISTOGRAM_LONG_TIMES_100(
        "MobileOmnibox.PressedClipboardSuggestionAge",
        ClipboardRecentContent::GetInstance()->GetClipboardContentAge());
  }
  delegate_->OnSelectedMatchForOpening(match, disposition, GURL(),
                                       std::u16string(), row);
}

void OmniboxPopupViewIOS::OnMatchSelectedForAppending(
    const AutocompleteMatch& match) {
  // Make a defensive copy of |match.fill_into_edit|, as CopyToOmnibox() will
  // trigger a new round of autocomplete and modify |match|.
  std::u16string fill_into_edit(match.fill_into_edit);

  // If the match is not a URL, append a whitespace to the end of it.
  if (AutocompleteMatch::IsSearchType(match.type)) {
    fill_into_edit.append(1, ' ');
  }

  delegate_->OnSelectedMatchForAppending(fill_into_edit);
}

void OmniboxPopupViewIOS::OnMatchSelectedForDeletion(
    const AutocompleteMatch& match) {
  model()->autocomplete_controller()->DeleteMatch(match);
}

void OmniboxPopupViewIOS::OnScroll() {
  delegate_->OnPopupDidScroll();
}
