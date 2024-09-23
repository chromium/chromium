// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fuzzer_env_with_web_state.h"

#import "base/command_line.h"
#import "base/i18n/icu_util.h"
#import "base/test/test_support_ios.h"
#import "base/test/test_timeouts.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

namespace web {

FuzzerEnvWithWebState::FuzzerEnvWithWebState() {
  CHECK(base::i18n::InitializeICU());

  base::CommandLine::Init(0, nullptr);
  TestTimeouts::Initialize();

  base::InitIOSTestMessageLoop();

  web_client_ = std::make_unique<web::FakeWebClient>();
  SetWebClient(web_client_.get());
  task_environment_ = std::make_unique<WebTaskEnvironment>();
  browser_state_ = std::make_unique<FakeBrowserState>();
  WebState::CreateParams params(browser_state_.get());
  web_state_ = WebState::Create(params);
}

FuzzerEnvWithWebState::~FuzzerEnvWithWebState() {}

web::WebState* FuzzerEnvWithWebState::web_state() {
  return web_state_.get();
}

}  // namespace web
