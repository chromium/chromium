// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/infobars/infobar_manager_impl.h"

#include "base/logging.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/web/public/navigation/navigation_context.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

infobars::InfoBarDelegate::NavigationDetails CreateNavigationDetails(
    web::NavigationItem* navigation_item,
    bool is_same_document) {
  infobars::InfoBarDelegate::NavigationDetails navigation_details;
  navigation_details.entry_id = navigation_item->GetUniqueID();
  const ui::PageTransition transition = navigation_item->GetTransitionType();
  navigation_details.is_navigation_to_different_page =
      ui::PageTransitionIsMainFrame(transition) && !is_same_document;
  // Default to false, since iOS callbacks do not specify if navigation was a
  // repace state navigation .
  navigation_details.did_replace_entry = false;
  navigation_details.is_reload =
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD);
  navigation_details.is_redirect = ui::PageTransitionIsRedirect(transition);
  navigation_details.is_form_submission =
      ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_FORM_SUBMIT);
  return navigation_details;
}

}  // namespace

InfoBarManagerImpl::InfoBarManagerImpl(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

InfoBarManagerImpl::~InfoBarManagerImpl() {
  ShutDown();

  // As the object can commit suicide, it is possible that its destructor
  // is called before WebStateDestroyed. In that case stop observing the
  // WebState.
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

int InfoBarManagerImpl::GetActiveEntryID() {
  web::NavigationItem* visible_item =
      web_state_->GetNavigationManager()->GetVisibleItem();
  return visible_item ? visible_item->GetUniqueID() : 0;
}

std::unique_ptr<infobars::InfoBar> InfoBarManagerImpl::CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  return ::CreateConfirmInfoBar(std::move(delegate));
}

void InfoBarManagerImpl::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  DCHECK_EQ(web_state_, web_state);
  // TODO(crbug.com/931841): Remove GetLastCommittedItem nil check once
  // HasComitted has been fixed.
  if (navigation_context->HasCommitted() &&
      web_state->GetNavigationManager()->GetLastCommittedItem()) {
    OnNavigation(CreateNavigationDetails(
        web_state->GetNavigationManager()->GetLastCommittedItem(),
        navigation_context->IsSameDocument()));
  }
}

void InfoBarManagerImpl::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  // The WebState is going away; be aggressively paranoid and delete this
  // InfoBarManagerImpl lest other parts of the system attempt to add infobars
  // or use it otherwise during the destruction. As this is the equivalent of
  // "delete this", returning from this function is the only safe thing to do.
  web_state_->RemoveUserData(UserDataKey());
}

void InfoBarManagerImpl::OpenURL(const GURL& url,
                                 WindowOpenDisposition disposition) {
  web::WebState::OpenURLParams params(url, web::Referrer(), disposition,
                                      ui::PAGE_TRANSITION_LINK,
                                      /*is_renderer_initiated=*/false);
  web_state_->OpenURL(params);
}

WEB_STATE_USER_DATA_KEY_IMPL(InfoBarManagerImpl)
