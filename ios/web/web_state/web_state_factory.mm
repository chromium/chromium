// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state.h"

#import <memory>

#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

/* static */
std::unique_ptr<WebState> WebState::Create(const CreateParams& params) {
  return std::make_unique<WebStateImpl>(params);
}

/* static */
std::unique_ptr<WebState> WebState::CreateWithStorageSession(
    const CreateParams& params,
    CRWSessionStorage* session_storage) {
  DCHECK(session_storage);
  return std::make_unique<WebStateImpl>(params, session_storage);
}

}  // namespace web
