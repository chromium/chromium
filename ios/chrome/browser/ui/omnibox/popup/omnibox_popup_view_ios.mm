// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"

#import <QuartzCore/QuartzCore.h>

#import <memory>
#import <string>

#import "base/check.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_suggestions_delegate.h"
#import "ios/chrome/browser/ui/omnibox/web_omnibox_edit_model_delegate.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/url_request/url_request_context_getter.h"

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

void OmniboxPopupViewIOS::UpdatePopupAppearance() {
  const AutocompleteResult& result = model()->result();

  [mediator_ updateWithResults:result];
}

bool OmniboxPopupViewIOS::IsOpen() const {
  return [mediator_ hasResults];
}

std::u16string OmniboxPopupViewIOS::GetAccessibleButtonTextForResult(
    size_t line) {
  return u"";
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

void OmniboxPopupViewIOS::OnMatchSelected(
    const AutocompleteMatch& selectedMatch,
    size_t row,
    WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));
  NewTabPageTabHelper* NTPTabHelper = NewTabPageTabHelper::FromWebState(
      static_cast<WebOmniboxEditModelDelegate*>(edit_model_->delegate())
          ->GetWebState());
  if (NTPTabHelper->IsActive()) {
    RecordHomeAction(IOSHomeActionType::kOmnibox,
                     NTPTabHelper->ShouldShowStartSurface());
  }

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, `match` and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = selectedMatch;

  if (match.type == AutocompleteMatchType::CLIPBOARD_URL ||
      match.type == AutocompleteMatchType::CLIPBOARD_TEXT) {
    // A search using clipboard link or text is activity that should indicate a
    // user that would be interested in setting Chrome as the default browser.
    LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeGeneral);
  }

  if (match.type == AutocompleteMatchType::CLIPBOARD_URL) {
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
  // Make a defensive copy of `match.fill_into_edit`, as CopyToOmnibox() will
  // trigger a new round of autocomplete and modify `match`.
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
