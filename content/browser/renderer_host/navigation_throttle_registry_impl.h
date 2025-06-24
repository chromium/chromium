// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_REGISTRY_IMPL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/safety_checks.h"
#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"

namespace content {

class NavigationHandle;
class NavigationRequest;
class NavigationThrottleRunner;

// The different event types that can be processed by NavigationThrottles.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// This type is also used in the UKM as set in the RecordDeferTimeUKM().
//
// LINT.IfChange(NavigationThrottleEvent)
enum class NavigationThrottleEvent {
  kNoEvent = 0,
  kWillStartRequest = 1,
  kWillRedirectRequest = 2,
  kWillFailRequest = 3,
  kWillProcessResponse = 4,
  kWillCommitWithoutUrlLoader = 5,
  kMaxValue = kWillCommitWithoutUrlLoader,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:NavigationThrottleEvent)


class CONTENT_EXPORT NavigationThrottleRegistryBase
    : public NavigationThrottleRegistry {
 public:
  ~NavigationThrottleRegistryBase() override;

  // Called when the NavigationThrottleRunner is done processing the navigation
  // event of type `event`. `result` is the final
  // NavigationThrottle::ThrottleCheckResult for this event.
  virtual void OnEventProcessed(
      NavigationThrottleEvent event,
      NavigationThrottle::ThrottleCheckResult result) = 0;

  // Returns the list of NavigationThrottles registered for this navigation.
  virtual std::vector<std::unique_ptr<NavigationThrottle>>& GetThrottles() = 0;

  // Returns the NavigationThrottle at the given `index`. The `index` should
  // be in a valid range.
  virtual NavigationThrottle& GetThrottleAtIndex(size_t index) = 0;
};

class CONTENT_EXPORT NavigationThrottleRegistryImpl
    : public NavigationThrottleRegistryBase {
  // Do not remove this macro!
  // The macro is maintained by the memory safety team.
  ADVANCED_MEMORY_SAFETY_CHECKS();

 public:
  explicit NavigationThrottleRegistryImpl(
      NavigationRequest* navigation_request);
  NavigationThrottleRegistryImpl(const NavigationThrottleRegistryImpl&) =
      delete;
  NavigationThrottleRegistryImpl& operator=(
      const NavigationThrottleRegistryImpl&) = delete;
  ~NavigationThrottleRegistryImpl() override;

  // Registers the appropriate NavigationThrottles for a "standard" navigation
  // (i.e., one with a URLLoader that goes through the
  // WillSendRequest/WillProcessResponse callback sequence).
  void RegisterNavigationThrottles();

  // Registers the appropriate NavigationThrottles for a navigation that can
  // immediately commit because no URLLoader is required (about:blank,
  // about:srcdoc, and most same-document navigations).
  void RegisterNavigationThrottlesForCommitWithoutUrlLoader();

  // Will call the appropriate NavigationThrottle function based on `event` on
  // all NavigationThrottles owned by this registry.
  void ProcessNavigationEvent(NavigationThrottleEvent event);

  // Unblocks the NavigationRequest that was deferred by `resuming_throttle`.
  // Once the NavigationThrottleRunner2 is enabled, multiple throttles may ask
  // to defer the navigation for the same NavigationThrottleEvent. The
  // underlying NavigationRequest will be resumed after all the throttles that
  // deferred the navigation have unblocked the navigation.
  void ResumeProcessingNavigationEvent(NavigationThrottle* resuiming_throttle);

  // Returns the throttles that are currently deferring the navigation.
  const std::set<NavigationThrottle*>& GetDeferringThrottles();

  // Returns the underlying NavigationThrottleRunner for tests to manipulate.
  // TODO(https://crbug.com/422003056): Remove this method, and hide the runner
  // interfaces from general code to decouple the runner. Once it is hidden,
  // drop the CONTENT_EXPORT from this class.
  NavigationThrottleRunner& GetNavigationThrottleRunnerForTesting();

  // Implements NavigationThrottleRegistry:
  NavigationHandle& GetNavigationHandle() override;
  void AddThrottle(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override;
  bool HasThrottle(const std::string& name) override;
  bool EraseThrottleForTesting(const std::string& name) override;

  // Implements NavigationThrottleRegistryBase:
  void OnEventProcessed(
      NavigationThrottleEvent event,
      NavigationThrottle::ThrottleCheckResult result) override;
  std::vector<std::unique_ptr<NavigationThrottle>>& GetThrottles() override;
  NavigationThrottle& GetThrottleAtIndex(size_t index) override;

 private:
  // Holds a reference to the NavigationRequest that owns this instance.
  const raw_ref<NavigationRequest> navigation_request_;

  // Owns the NavigationThrottles associated with this navigation, and is
  // responsible for notifying them about the various navigation events.
  std::unique_ptr<NavigationThrottleRunner> navigation_throttle_runner_;

  // A list of Throttles registered for this navigation.
  std::vector<std::unique_ptr<NavigationThrottle>> throttles_;

  // The throttles that are currently deferring the navigation if it runs with
  // the v1 runner. This is needed to adopt the v1 interface to the registry's
  // GetDeferringThrottles(). This is lazily initialized on every
  // GetDeferringThrottles() call.
  // TODO(https://crbug.com/.422003056): Explore more efficient approach, i.e.
  // the runner notifies the registry to update this set.
  std::set<NavigationThrottle*> deferring_throttles_in_v1_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_REGISTRY_IMPL_H_
