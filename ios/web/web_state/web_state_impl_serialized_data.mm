// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_state_impl_serialized_data.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "ios/web/public/web_state_observer.h"

namespace web {

WebStateImpl::SerializedData::SerializedData(WebStateImpl* owner,
                                             const CreateParams& create_params,
                                             CRWSessionStorage* session_storage)
    : owner_(owner),
      create_params_(create_params),
      session_storage_(session_storage) {}

WebStateImpl::SerializedData::~SerializedData() = default;

void WebStateImpl::SerializedData::TearDown() {
  for (auto& observer : observers())
    observer.WebStateDestroyed(owner_);
  for (auto& observer : policy_deciders())
    observer.WebStateDestroyed();
  for (auto& observer : policy_deciders())
    observer.ResetWebState();
}

WebState::CreateParams WebStateImpl::SerializedData::GetCreateParams() const {
  return create_params_;
}

CRWSessionStorage* WebStateImpl::SerializedData::GetSessionStorage() const {
  return session_storage_;
}

BrowserState* WebStateImpl::SerializedData::GetBrowserState() const {
  return create_params_.browser_state;
}

}  // namespace web
