// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/supervised_user_error_container.h"

#import "base/memory/ptr_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

WEB_STATE_USER_DATA_KEY_IMPL(SupervisedUserErrorContainer)

SupervisedUserErrorContainer::SupervisedUserErrorContainer(
    web::WebState* web_state) {}

SupervisedUserErrorContainer::~SupervisedUserErrorContainer() = default;

SupervisedUserErrorContainer::SupervisedUserErrorInfo::SupervisedUserErrorInfo(
    const GURL& request_url,
    bool is_main_frame,
    bool is_already_requested,
    supervised_user::FilteringBehaviorReason filtering_behavior_reason) {
  request_url_ = request_url;
  is_main_frame_ = is_main_frame;
  is_already_requested_ = is_already_requested;
  filtering_behavior_reason_ = filtering_behavior_reason;
}
void SupervisedUserErrorContainer::SetSupervisedUserErrorInfo(
    std::unique_ptr<SupervisedUserErrorInfo> error_info) {
  supervised_user_error_info_ = std::move(error_info);
}

SupervisedUserErrorContainer::SupervisedUserErrorInfo&
SupervisedUserErrorContainer::GetSupervisedUserErrorInfo() {
  return *supervised_user_error_info_.get();
}
