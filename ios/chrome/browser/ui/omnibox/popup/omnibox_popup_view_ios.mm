// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"
#include "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_suggestions_delegate.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/geometry/rect.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

OmniboxPopupViewIOS::OmniboxPopupViewIOS(
    OmniboxEditModel* edit_model,
    OmniboxPopupViewSuggestionsDelegate* delegate)
    : model_(new OmniboxPopupModel(this, edit_model)), delegate_(delegate) {
  DCHECK(delegate);
  DCHECK(edit_model);
}

OmniboxPopupViewIOS::~OmniboxPopupViewIOS() {
  // Destroy the model, in case it tries to call back into us when destroyed.
  model_.reset();
}

// Set left image to globe or magnifying glass depending on which autocomplete
// option is highlighted.
void OmniboxPopupViewIOS::UpdateEditViewIcon() {
  const AutocompleteResult& result = model_->result();
  const AutocompleteMatch& match = result.match_at(model_->selected_line());
  delegate_->OnTopmostSuggestionImageChanged(match.type);
}

void OmniboxPopupViewIOS::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();

  [mediator_ updateWithResults:result];
  if ([mediator_ isOpen]) {
    UpdateEditViewIcon();
  }

  delegate_->OnResultsChanged(result);
}

bool OmniboxPopupViewIOS::IsOpen() const {
  return [mediator_ hasResults];
}

OmniboxPopupModel* OmniboxPopupViewIOS::model() const {
  return model_.get();
}

#pragma mark - OmniboxPopupProvider

bool OmniboxPopupViewIOS::IsPopupOpen() {
  return [mediator_ isOpen];
}

void OmniboxPopupViewIOS::SetTextAlignment(NSTextAlignment alignment) {
  [mediator_ setTextAlignment:alignment];
}

#pragma mark - OmniboxPopupViewControllerDelegate

bool OmniboxPopupViewIOS::IsStarredMatch(const AutocompleteMatch& match) const {
  return model_->IsStarredMatch(match);
}

void OmniboxPopupViewIOS::OnMatchHighlighted(size_t row) {
  model_->SetSelectedLine(row, false, true);
  if ([mediator_ isOpen]) {
    UpdateEditViewIcon();
  }
}

void OmniboxPopupViewIOS::OnMatchSelected(
    const AutocompleteMatch& selectedMatch,
    size_t row) {
  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = selectedMatch;

  if (match.type == AutocompleteMatchType::CLIPBOARD) {
    base::RecordAction(UserMetricsAction("MobileOmniboxClipboardToURL"));
    UMA_HISTOGRAM_LONG_TIMES_100(
        "MobileOmnibox.PressedClipboardSuggestionAge",
        ClipboardRecentContent::GetInstance()->GetClipboardContentAge());
  }
  delegate_->OnSelectedMatchForOpening(match, disposition, GURL(),
                                       base::string16(), row);
}

void OmniboxPopupViewIOS::OnMatchSelectedForAppending(
    const AutocompleteMatch& match) {
  // Make a defensive copy of |match.fill_into_edit|, as CopyToOmnibox() will
  // trigger a new round of autocomplete and modify |match|.
  base::string16 fill_into_edit(match.fill_into_edit);

  // If the match is not a URL, append a whitespace to the end of it.
  if (AutocompleteMatch::IsSearchType(match.type)) {
    fill_into_edit.append(1, ' ');
  }

  delegate_->OnSelectedMatchForAppending(fill_into_edit);
}

void OmniboxPopupViewIOS::OnMatchSelectedForDeletion(
    const AutocompleteMatch& match) {
  model_->autocomplete_controller()->DeleteMatch(match);
}

void OmniboxPopupViewIOS::OnScroll() {
  delegate_->OnPopupDidScroll();
}
