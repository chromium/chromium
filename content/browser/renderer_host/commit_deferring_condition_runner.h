// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_RUNNER_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_RUNNER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prerender/prerender_commit_deferring_condition.h"
#include "content/common/content_export.h"

namespace content {

class NavigationRequest;

// Helper class used to defer an otherwise fully-prepared navigation (i.e.
// followed all redirects, passed all NavigationThrottle checks) from
// proceeding until all preconditions are met.
//
// Clients subclass the CommitDeferringCondition class to wait on a commit
// blocking condition to be resolved and invoke the callback when it's ready.
// The client should register their subclass class in
// RegisterDeferringConditions().  Each condition is run in order, waiting on
// that condition to call Resume() before starting the next one. Once the final
// condition is completed, the navigation is resumed to commit.
//
// This mechanism is not applied to about:blank or same-document navigations.
//
// CommitDeferringCondition vs. NavigationThrottle: At first glance this
// mechanism may seem redundant to using a NavigationThrottle (and deferring in
// WillProcessResponse). However, the behavior will differ on pages initially
// loaded into a non-primary FrameTree (e.g. prerendering or BFCached page).
// In these cases NavigationThrottles will run only when the page was loading,
// they will not get a chance to intervene when it is being activated to the
// primary FrameTree (i.e. user navigates to a prerendered page). If the
// navigation needs to be deferred during such activations, a
// CommitDeferringCondition must be used.  It runs both when the navigation is
// loading and when a navigation activates into the primary FrameTree.
class CONTENT_EXPORT CommitDeferringConditionRunner {
 public:
  class Delegate {
   public:
    // Called after all conditions run. `candidate_prerender_frame_tree_node_id`
    // is used for querying the PrerenderHost that this navigation will try to
    // activate. See comments on `candidate_prerender_frame_tree_node_id_` for
    // details.
    virtual void OnCommitDeferringConditionChecksComplete(
        CommitDeferringCondition::NavigationType navigation_type,
        std::optional<FrameTreeNodeId>
            candidate_prerender_frame_tree_node_id) = 0;
  };

  // Creates the runner and adds all the conditions in
  // RegisterDeferringConditions. `candidate_prerender_frame_tree_node_id`
  // is used for querying the PrerenderHost that this navigation will try to
  // activate. See comments on `candidate_prerender_frame_tree_node_id_` for
  // details.
  static std::unique_ptr<CommitDeferringConditionRunner> Create(
      NavigationRequest& navigation_request,
      CommitDeferringCondition::NavigationType navigation_type,
      std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id);

  CommitDeferringConditionRunner(const CommitDeferringConditionRunner&) =
      delete;
  CommitDeferringConditionRunner& operator=(
      const CommitDeferringConditionRunner&) = delete;

  ~CommitDeferringConditionRunner();

  // Call to start iterating through registered CommitDeferringConditions. This
  // calls OnCommitDeferringConditionChecksComplete on the |delegate_| when all
  // conditions have been resolved. This may happen either synchronously or
  // asynchronously.
  void ProcessChecks();

  // Call to register all deferring conditions. This should be called when
  // NavigationState < WILL_START_NAVIGATION for prerendered page activation, or
  // NavigationState == WILL_PROCESS_RESPONSE for other navigations.
  void RegisterDeferringConditions(NavigationRequest& navigation_request);

  // Installs a callback to generate a deferring condition. Installed callbacks
  // are called every time RegisterDeferringConditions() is called. Generated
  // conditions are added to `conditions_` and run after all regularly
  // registered conditions. This is typically used for adding a condition before
  // NavigationRequest is created.
  using ConditionGenerator =
      base::RepeatingCallback<std::unique_ptr<CommitDeferringCondition>(
          NavigationHandle&,
          CommitDeferringCondition::NavigationType)>;

  // Specifies whether a ConditionGenerator installs its condition to run
  // before existing conditions or after. Note: generators are run in the order
  // in which they are added.
  enum class InsertOrder { kBefore, kAfter };

  // Returns a generator id that is used for uninstalling the generator.
  static int InstallConditionGeneratorForTesting(ConditionGenerator generator,
                                                 InsertOrder order);

  // `generator_id` should be an identifier returned by
  // InstallConditionGeneratorForTesting().
  static void UninstallConditionGeneratorForTesting(int generator_id);

  // Used in tests to inject mock conditions.
  void AddConditionForTesting(
      std::unique_ptr<CommitDeferringCondition> condition);

  // Returns the condition that's currently causing the navigation commit to be
  // deferred. If no condition is currently deferred, returns nullptr.
  CommitDeferringCondition* GetDeferringConditionForTesting() const;

 private:
  friend class CommitDeferringConditionRunnerTest;

  CommitDeferringConditionRunner(
      Delegate& delegate,
      CommitDeferringCondition::NavigationType navigation_type,
      std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id);

  // Called asynchronously to resume iterating through
  // CommitDeferringConditions after one has been deferred. A callback for this
  // method is passed into each condition when WillCommitNavigation is called.
  void ResumeProcessing();

  void ProcessConditions();
  void AddCondition(std::unique_ptr<CommitDeferringCondition> condition,
                    InsertOrder order = InsertOrder::kAfter);

  std::vector<std::unique_ptr<CommitDeferringCondition>> conditions_;

  // This class is owned by its delegate (the NavigationRequest) so it's safe
  // to keep a reference to it.
  const raw_ref<Delegate> delegate_;

  // Used for distiguishing prerendered page activation from other navigations.
  // This is needed as IsPageActivation() and IsPrerenderedPageActivation() on
  // NavigationRequest are not available yet while CommitDeferringCondition is
  // running.
  const CommitDeferringCondition::NavigationType navigation_type_;

  // Used for querying PrerenderHost this navigation will try to activate.
  // This is valid only when `navigation_type_` is kPrerenderedPageActivation.
  // This is needed as PrerenderHost hasn't been reserved and
  // prerender_frame_tree_node_id() on NavigationRequest is not available yet
  // while CommitDeferringCondition is running.
  const std::optional<FrameTreeNodeId> candidate_prerender_frame_tree_node_id_;

  // True when we're blocked waiting on a call to ResumeProcessing.
  bool is_deferred_ = false;

  base::WeakPtrFactory<CommitDeferringConditionRunner> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMMIT_DEFERRING_CONDITION_RUNNER_H_
