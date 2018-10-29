// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/web_omnibox_edit_controller_impl.h"

#include "components/omnibox/browser/toolbar_model.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_url_loader.h"
#import "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WebOmniboxEditControllerImpl::WebOmniboxEditControllerImpl(
    id<LocationBarDelegate> delegate)
    : delegate_(delegate){
          // TODO(crbug.com/818645): add security icon and its a11y labels
      };

WebOmniboxEditControllerImpl::~WebOmniboxEditControllerImpl(){};

web::WebState* WebOmniboxEditControllerImpl::GetWebState() {
  return [delegate_ webState];
}

void WebOmniboxEditControllerImpl::OnKillFocus() {
  // TODO(crbug.com/818648): disable fullscreen in LocationBarMediator.
  [delegate_ locationBarHasResignedFirstResponder];
}

void WebOmniboxEditControllerImpl::OnSetFocus() {
  // TODO(crbug.com/818648): reenable fullscreen in LocationBarMediator.
  [delegate_ locationBarHasBecomeFirstResponder];
}

void WebOmniboxEditControllerImpl::OnAutocompleteAccept(
    const GURL& destination_url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp) {
  if (destination_url.is_valid()) {
    transition = ui::PageTransitionFromInt(
        transition | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    [URLLoader_ loadGURLFromLocationBar:destination_url transition:transition];
  }
}

void WebOmniboxEditControllerImpl::OnInputInProgress(bool in_progress) {
  if ([delegate_ toolbarModel])
    [delegate_ toolbarModel]->set_input_in_progress(in_progress);
  // TODO(crbug.com/818649): see if this is really used.
  if (in_progress)
    [delegate_ locationBarBeganEdit];
}

void WebOmniboxEditControllerImpl::OnChanged() {
  // Called when anything is changed. Since the Mediator already observes the
  // WebState for security status changes, no need to do anything.
  // TODO(crbug.com/818645): update the security icon in LocationBarMediator.
}

ToolbarModel* WebOmniboxEditControllerImpl::GetToolbarModel() {
  return [delegate_ toolbarModel];
}

const ToolbarModel* WebOmniboxEditControllerImpl::GetToolbarModel() const {
  return [delegate_ toolbarModel];
}
