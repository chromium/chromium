// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FUZZER_ENV_WITH_WEB_STATE_H_
#define IOS_WEB_PUBLIC_TEST_FUZZER_ENV_WITH_WEB_STATE_H_

#include <memory>

#include "base/at_exit.h"
#import "ios/web/public/browser_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

namespace web {

// A class with required web task environment and a `WebState*| set up in its
// constructor. It should be used as a function level static var in the
// libFuzzer `LLVMFuzzerTestOneInput` to run the environment set up once. It can
// be extended to add more set ups in subclass's constructor.
class FuzzerEnvWithWebState {
 public:
  FuzzerEnvWithWebState();
  ~FuzzerEnvWithWebState();

  web::WebState* web_state();

 private:
  base::AtExitManager at_exit_manager_;
  std::unique_ptr<WebClient> web_client_;
  std::unique_ptr<WebTaskEnvironment> task_environment_;
  std::unique_ptr<BrowserState> browser_state_;
  std::unique_ptr<web::WebState> web_state_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FUZZER_ENV_WITH_WEB_STATE_H_
