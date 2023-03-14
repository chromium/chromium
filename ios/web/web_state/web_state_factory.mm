// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state.h"

#import <memory>

#import "build/blink_buildflags.h"
#import "ios/web/web_state/web_state_impl.h"

#if BUILDFLAG(USE_BLINK)
#import "ios/web/content/web_state/content_web_state.h"
#endif  // USE_BLINK

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

/* static */
std::unique_ptr<WebState> WebState::Create(const CreateParams& params) {
#if BUILDFLAG(USE_BLINK)
  return std::make_unique<ContentWebState>(params);
#else
  return std::make_unique<WebStateImpl>(params);
#endif  // USE_BLINK
}

/* static */
std::unique_ptr<WebState> WebState::CreateWithStorageSession(
    const CreateParams& params,
    CRWSessionStorage* session_storage) {
  DCHECK(session_storage);
#if BUILDFLAG(USE_BLINK)
  return std::make_unique<ContentWebState>(params);
#else
  return std::make_unique<WebStateImpl>(params, session_storage);
#endif  // USE_BLINK
}

}  // namespace web
