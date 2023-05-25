// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/web_omnibox_edit_model_delegate_impl.h"

#import "components/omnibox/browser/location_bar_model.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_controller_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebOmniboxEditModelDelegateImpl::WebOmniboxEditModelDelegateImpl(
    id<OmniboxControllerDelegate> delegate,
    id<OmniboxFocusDelegate> focus_delegate)
    : delegate_(delegate), focus_delegate_(focus_delegate) {}

WebOmniboxEditModelDelegateImpl::~WebOmniboxEditModelDelegateImpl() {}

web::WebState* WebOmniboxEditModelDelegateImpl::GetWebState() {
  return [delegate_ webState];
}

void WebOmniboxEditModelDelegateImpl::OnKillFocus() {
  [focus_delegate_ omniboxDidResignFirstResponder];
}

void WebOmniboxEditModelDelegateImpl::OnSetFocus() {
  [focus_delegate_ omniboxDidBecomeFirstResponder];
}

void WebOmniboxEditModelDelegateImpl::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    bool destination_url_entered_with_http_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match,
    IDNA2008DeviationCharacter deviation_char_in_hostname) {
  if (destination_url.is_valid()) {
    transition = ui::PageTransitionFromInt(
        transition | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    [URLLoader_ loadGURLFromLocationBar:destination_url
                                   postContent:post_content
                                    transition:transition
                                   disposition:disposition
        destination_url_entered_without_scheme:
            destination_url_entered_without_scheme];
  }
}

void WebOmniboxEditModelDelegateImpl::OnChanged() {
  // Called when anything is changed. Since the Mediator already observes the
  // WebState for security status changes, no need to do anything.
}

LocationBarModel* WebOmniboxEditModelDelegateImpl::GetLocationBarModel() {
  return [delegate_ locationBarModel];
}

const LocationBarModel* WebOmniboxEditModelDelegateImpl::GetLocationBarModel()
    const {
  return [delegate_ locationBarModel];
}
