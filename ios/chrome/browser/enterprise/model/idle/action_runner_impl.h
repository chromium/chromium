// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_RUNNER_IMPL_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_RUNNER_IMPL_H_

#import "ios/chrome/browser/enterprise/model/idle/action_runner.h"

#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

namespace enterprise_idle {

class ActionRunnerImpl : public ActionRunner {
 public:
  ActionRunnerImpl(ChromeBrowserState* chrome_browser_state);
  void Run() override;
};

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_RUNNER_IMPL_H_
