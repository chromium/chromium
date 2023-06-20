// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/web_location_bar_impl.h"

#import "components/omnibox/browser/location_bar_model.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_controller_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebLocationBarImpl::WebLocationBarImpl(id<OmniboxControllerDelegate> delegate,
                                       id<OmniboxFocusDelegate> focus_delegate)
    : delegate_(delegate), focus_delegate_(focus_delegate) {}

WebLocationBarImpl::~WebLocationBarImpl() {}

web::WebState* WebLocationBarImpl::GetWebState() {
  return [delegate_ webState];
}

void WebLocationBarImpl::OnKillFocus() {
  [focus_delegate_ omniboxDidResignFirstResponder];
}

void WebLocationBarImpl::OnSetFocus() {
  [focus_delegate_ omniboxDidBecomeFirstResponder];
}

void WebLocationBarImpl::OnNavigate(const GURL& destination_url,
                                    TemplateURLRef::PostContent* post_content,
                                    WindowOpenDisposition disposition,
                                    ui::PageTransition transition,
                                    bool destination_url_entered_without_scheme,
                                    const AutocompleteMatch& match) {
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

LocationBarModel* WebLocationBarImpl::GetLocationBarModel() {
  return [delegate_ locationBarModel];
}
