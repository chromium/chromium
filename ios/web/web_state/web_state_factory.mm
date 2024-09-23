// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state.h"

#import <memory>

#import "base/check.h"
#import "base/functional/callback.h"
#import "build/blink_buildflags.h"
#import "ios/web/public/session/proto/metadata.pb.h"

#if BUILDFLAG(USE_BLINK)
#import "ios/web/content/web_state/content_web_state.h"
#else
#import "ios/web/web_state/web_state_impl.h"
#endif  // USE_BLINK

namespace web {

#if BUILDFLAG(USE_BLINK)
using ConcreteWebStateType = ContentWebState;
#else
using ConcreteWebStateType = WebStateImpl;
#endif  // USE_BLINK

/* static */
std::unique_ptr<WebState> WebState::Create(const CreateParams& params) {
  return std::make_unique<ConcreteWebStateType>(params);
}

/* static */
std::unique_ptr<WebState> WebState::CreateWithStorageSession(
    const CreateParams& params,
    CRWSessionStorage* session_storage,
    NativeSessionFetcher session_fetcher) {
  DCHECK(session_storage);
  return std::make_unique<ConcreteWebStateType>(params, session_storage,
                                                std::move(session_fetcher));
}

/* static */
std::unique_ptr<WebState> WebState::CreateWithStorage(
    BrowserState* browser_state,
    WebStateID unique_identifier,
    proto::WebStateMetadataStorage metadata,
    WebStateStorageLoader storage_loader,
    NativeSessionFetcher session_fetcher) {
  return std::make_unique<ConcreteWebStateType>(
      browser_state, unique_identifier, std::move(metadata),
      std::move(storage_loader), std::move(session_fetcher));
}

}  // namespace web
