// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_RUNNER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_RUNNER_H_

namespace enterprise_idle {

// Runs actions specified by the IdleTimeoutActions policy. Wrapper around
// Action that handles asynchronicity, and runs them in order of priority.
// One per profile. Owned by `IdleService`.
class ActionRunner {
 public:
  explicit ActionRunner() = default;
  virtual ~ActionRunner() = default;
  ActionRunner(const ActionRunner&) = delete;
  ActionRunner& operator=(const ActionRunner&) = delete;
  ActionRunner(ActionRunner&&) = delete;
  ActionRunner& operator=(ActionRunner&&) = delete;

  // Runs all the actions, in order of priority. Actions are run sequentially,
  // not in parallel. If an action fails for whatever reason, skips the
  // remaining actions.
  virtual void Run() = 0;
};

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_RUNNER_H_
