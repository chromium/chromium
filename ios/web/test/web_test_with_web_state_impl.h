// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_WEB_TEST_WITH_WEB_STATE_IMPL_H_
#define IOS_WEB_TEST_WEB_TEST_WITH_WEB_STATE_IMPL_H_

#import <memory>
#import <utility>

#import "base/types/pass_key.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/web_state/web_state_impl.h"

namespace web {

// Access to WebStateImpl is restricted to the following classes. Most of
// the tests should use WebState instead of WebStateImpl and thus should
// use WebTestWithWebState instead of WebTestWithWebStateImpl.
class NavigationManagerSerialisationTest;
class WebStateImplTest;

// A test fixture for web tests that needs to create WebStateImpl instance.
class WebTestWithWebStateImpl : public WebTest {
 public:
  using PassKey =
      base::PassKey<NavigationManagerSerialisationTest, WebStateImplTest>;

  // Public constructor requires a pass key to restrict allowed sub-classes.
  WebTestWithWebStateImpl(PassKey pass_key);

  // Public constructor requires a pass key to restrict allowed sub-classes.
  WebTestWithWebStateImpl(PassKey pass_key,
                          WebTaskEnvironment::MainThreadType main_thread_type);

  // Public constructor requires a pass key to restrict allowed sub-classes.
  WebTestWithWebStateImpl(PassKey pass_key,
                          std::unique_ptr<web::WebClient> web_client,
                          WebTaskEnvironment::MainThreadType main_thread_type);

  // Creates a WebStateImpl with the given parameters.
  template <typename... Args>
  std::unique_ptr<WebStateImpl> CreateWebStateImpl(Args&&... args) {
    auto web_state = std::make_unique<WebStateImpl>(
        base::PassKey<WebTestWithWebStateImpl>{}, std::forward<Args>(args)...);
    StartObservingWebStateForRenderProcessGone(web_state.get());
    return web_state;
  }
};

}  // namespace web

#endif  // IOS_WEB_TEST_WEB_TEST_WITH_WEB_STATE_IMPL_H_
