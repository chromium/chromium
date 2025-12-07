// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_H_

#import <memory>
#import <queue>
#import <vector>

#import "base/containers/flat_set.h"
#import "base/containers/span.h"
#import "base/functional/callback.h"
#import "base/no_destructor.h"
#import "build/build_config.h"
#import "components/enterprise/idle/action_type.h"
#import "ios/chrome/browser/browsing_data/model/browsing_data_remover.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

namespace enterprise_idle {

// An action that should Run() when a given event happens. See the
// IdleTimeoutActions policy.
class Action {
 public:
  using Continuation = base::OnceCallback<void(bool succeeded)>;

  explicit Action(int priority);
  virtual ~Action();

  Action(const Action&) = delete;
  Action& operator=(const Action&) = delete;
  Action(Action&&) = delete;
  Action& operator=(Action&&) = delete;

  // Runs this action on `browser_state`, which may be asynchronous. When it's
  // done, runs `continuation` with the result.
  virtual void Run(ProfileIOS* profile, Continuation continuation) = 0;

  int priority() const { return priority_; }

 private:
  const int priority_;
};

// A factory that takes a list of `ActionType` and converts it to a
// `priority_queue<Action>`. See Build().
class ActionFactory {
 public:
  struct CompareActionsByPriority {
    bool operator()(const std::unique_ptr<Action>& a,
                    const std::unique_ptr<Action>& b) const;
  };

  using ActionQueue = std::priority_queue<std::unique_ptr<Action>,
                                          std::vector<std::unique_ptr<Action>>,
                                          CompareActionsByPriority>;
  ActionFactory();
  ActionFactory(const ActionFactory&) = delete;
  ActionFactory& operator=(const ActionFactory&) = delete;
  ActionFactory(ActionFactory&&) = delete;
  ActionFactory& operator=(ActionFactory&&) = delete;
  virtual ~ActionFactory();

  // Converts the pref/policy value to a priority_queue<> of actions.
  virtual ActionQueue Build(
      const std::vector<ActionType>& action_types,
      BrowsingDataRemover* main_browsing_data_remover,
      BrowsingDataRemover* incognito_browsing_data_remover);
};

}  // namespace enterprise_idle

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_MODEL_IDLE_ACTION_H_
