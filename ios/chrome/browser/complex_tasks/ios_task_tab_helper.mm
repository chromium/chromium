// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/complex_tasks/ios_task_tab_helper.h"

#include <string>

#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Determines if transition means the navigation will be in the same task
// or start a new task. For example, clicking on a link continues the
// current task. Starting a new search in the omnibox starts a new task.
bool DoesTransitionContinueTask(ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_SUBFRAME) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_MANUAL_SUBFRAME) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_FORM_SUBMIT) ||
         transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK;
}
}  // namespace

IOSTaskTabHelper::IOSTaskTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state_->AddObserver(this);
}

IOSTaskTabHelper::~IOSTaskTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void IOSTaskTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  web::NavigationManager* navigation_manager =
      web_state->GetNavigationManager();
  if (navigation_manager->GetLastCommittedItem()) {
    prev_item_unique_id_ =
        navigation_manager->GetLastCommittedItem()->GetUniqueID();
  }
}

void IOSTaskTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  web::NavigationManager* navigation_manager =
      web_state->GetNavigationManager();
  web::NavigationItem* last_committed_item =
      navigation_manager->GetLastCommittedItem();
  if (!last_committed_item)
    return;
  IOSContentRecordTaskId ios_content_record_task_id;
  ios_content_record_task_id.set_task_id(
      last_committed_item->GetTimestamp().since_origin().InMicroseconds());

  if (DoesTransitionContinueTask(navigation_context->GetPageTransition())) {
    if (prev_item_unique_id_ != -1) {
      IOSContentRecordTaskId prev =
          ios_content_record_task_id_map_[prev_item_unique_id_];
      ios_content_record_task_id.set_parent_task_id(prev.task_id());
      ios_content_record_task_id.set_root_task_id(prev.root_task_id());
    } else {
      ios_content_record_task_id.set_root_task_id(
          ios_content_record_task_id.task_id());
    }
  } else {
    ios_content_record_task_id.set_root_task_id(
        ios_content_record_task_id.task_id());
  }
  ios_content_record_task_id_map_.emplace(last_committed_item->GetUniqueID(),
                                          ios_content_record_task_id);
  prev_item_unique_id_ = -1;
}

void IOSTaskTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
}

const IOSContentRecordTaskId* IOSTaskTabHelper::GetContextRecordTaskId(
    int nav_id) const {
  auto result = ios_content_record_task_id_map_.find(nav_id);
  return result != ios_content_record_task_id_map_.end() ? &result->second
                                                         : nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(IOSTaskTabHelper)
