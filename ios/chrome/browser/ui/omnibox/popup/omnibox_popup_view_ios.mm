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
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/browser/omnibox_popup_selection.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/shared/metrics/home_metrics.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_suggestions_delegate.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/url_request/url_request_context_getter.h"

using base::UserMetricsAction;

OmniboxPopupViewIOS::OmniboxPopupViewIOS(
    OmniboxController* controller,
    OmniboxPopupViewSuggestionsDelegate* delegate)
    : OmniboxPopupView(controller),
      delegate_(delegate) {
  DCHECK(delegate);
  DCHECK(controller);
  model()->set_popup_view(this);
}

OmniboxPopupViewIOS::~OmniboxPopupViewIOS() {
  model()->set_popup_view(nullptr);
}

void OmniboxPopupViewIOS::UpdatePopupAppearance() {
  [mediator_
      updateWithResults:controller()->autocomplete_controller()->result()];
}

bool OmniboxPopupViewIOS::IsOpen() const {
  return [mediator_ hasResults];
}

std::u16string OmniboxPopupViewIOS::GetAccessibleButtonTextForResult(
    size_t line) {
  return u"";
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

void OmniboxPopupViewIOS::SetHasThumbnail(bool has_thumbnail) {
  [mediator_ setHasThumbnail:has_thumbnail];
}

#pragma mark - OmniboxPopupViewControllerDelegate

bool OmniboxPopupViewIOS::IsStarredMatch(const AutocompleteMatch& match) const {
  return model()->IsStarredMatch(match);
}

void OmniboxPopupViewIOS::OnMatchSelected(
    const AutocompleteMatch& selectedMatch,
    size_t row,
    WindowOpenDisposition disposition) {
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, `match` and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = selectedMatch;

  if (match.type == AutocompleteMatchType::CLIPBOARD_URL) {
    // TODO(crbug.com/326989399): MobileOmniboxClipboardToURL action is not
    // defined in actions.xml
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
  controller()->autocomplete_controller()->DeleteMatch(match);
}

void OmniboxPopupViewIOS::OnScroll() {
  delegate_->OnPopupDidScroll();
}

void OmniboxPopupViewIOS::OnCallActionTap() {
  delegate_->OnCallActionTap();
}
