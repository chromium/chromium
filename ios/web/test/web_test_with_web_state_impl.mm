// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/web_test_with_web_state_impl.h"

#import "ios/web/public/web_client.h"

namespace web {

WebTestWithWebStateImpl::WebTestWithWebStateImpl(PassKey pass_key)
    : WebTest() {}

WebTestWithWebStateImpl::WebTestWithWebStateImpl(
    PassKey pass_key,
    WebTaskEnvironment::MainThreadType main_thread_type)
    : WebTest(main_thread_type) {}

WebTestWithWebStateImpl::WebTestWithWebStateImpl(
    PassKey pass_key,
    std::unique_ptr<web::WebClient> web_client,
    WebTaskEnvironment::MainThreadType main_thread_type)
    : WebTest(std::move(web_client), main_thread_type) {}

}  // namespace web
