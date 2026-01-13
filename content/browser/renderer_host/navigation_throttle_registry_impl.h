// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_REGISTRY_IMPL_H_

#include <optional>
#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/safety_checks.h"
#include "base/memory/weak_ptr.h"
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

// This is an abstract class that collaborates with
// NavigationThrottleRegistryBase that owns the set of NavigationThrottles added
// to an underlying navigation, and is responsible for calling the various sets
// of events on its NavigationThrottles, and notifying its delegate about the
// results of said events.
class NavigationThrottleRunnerBase {
 public:
  virtual ~NavigationThrottleRunnerBase() = default;

  // Will call the appropriate NavigationThrottle function based on `event` on
  // all NavigationThrottles owned by this NavigationThrottleRunner.
  virtual void ProcessNavigationEvent(NavigationThrottleEvent event) = 0;

  // Resumes calling the appropriate NavigationThrottle functions for the
  // current processing event on all NavigationThrottles that have not yet been
  // notified.
  // `resuming_throttle` is the NavigationThrottle that asks for navigation
  // event processing to be resumed; it should be the one currently deferring
  // the navigation.
  virtual void ResumeProcessingNavigationEvent(
      NavigationThrottle* resuming_throttle) = 0;
};

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

  // Called when the NavigationThrottleRunner is about to defer the navigation
  // per a request from the given `deferring_throttle`.
  virtual void OnDeferProcessingNavigationEvent(
      NavigationThrottle* deferring_throttle) = 0;

  // Returns the list of NavigationThrottles registered for this navigation.
  virtual std::vector<std::unique_ptr<NavigationThrottle>>& GetThrottles() = 0;

  // Returns the NavigationThrottle at the given `index`. The `index` should
  // be in a valid range.
  virtual NavigationThrottle& GetThrottleAtIndex(size_t index) = 0;

  // Returns the throttles that are currently deferring the navigation.
  virtual const std::set<NavigationThrottle*>& GetDeferringThrottles()
      const = 0;
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

  // Sets a callback to be called when the navigation is deferred for the first
  // time.
  void SetFirstDeferralCallbackForTesting(base::OnceClosure callback);

  // Implements NavigationThrottleRegistry:
  NavigationHandle& GetNavigationHandle() override;
  void AddThrottle(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override;
  bool HasThrottle(const std::string& name) override;
  bool EraseThrottleForTesting(const std::string& name) override;
  bool IsHTTPOrHTTPS() override;

  // Implements NavigationThrottleRegistryBase:
  void OnEventProcessed(
      NavigationThrottleEvent event,
      NavigationThrottle::ThrottleCheckResult result) override;
  std::vector<std::unique_ptr<NavigationThrottle>>& GetThrottles() override;
  void OnDeferProcessingNavigationEvent(
      NavigationThrottle* deferring_throttle) override;
  NavigationThrottle& GetThrottleAtIndex(size_t index) override;
  const std::set<NavigationThrottle*>& GetDeferringThrottles() const override;

 private:
  // Holds a reference to the NavigationRequest that owns this instance.
  const raw_ref<NavigationRequest> navigation_request_;

  // WeakPtr version of `navigation_request_` to prevent calling in cases where
  // the NavigationRequest is already deleted.
  // TODO(crbug.com/470054231): Remove once this is confirmed to not be needed.
  base::WeakPtr<NavigationRequest> weak_navigation_request_;

  // Owns the NavigationThrottles associated with this navigation, and is
  // responsible for notifying them about the various navigation events.
  std::unique_ptr<NavigationThrottleRunnerBase> navigation_throttle_runner_;

  // A list of Throttles registered for this navigation.
  std::vector<std::unique_ptr<NavigationThrottle>> throttles_;

  // The throttles that are currently deferring the navigation.
  std::set<NavigationThrottle*> deferring_throttles_;

  // This is used in an experiment to cache frequently used navigation
  // attributes.
  // TODO(https://424460302): Remove this once the experiment completes, and
  // move the cache to GURL if it's successful.
  std::optional<bool> is_http_or_https_;

  // A callback to be called when the navigation is deferred for the first time.
  base::OnceClosure first_deferral_callback_for_testing_;

  base::WeakPtrFactory<NavigationThrottleRegistryImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_THROTTLE_REGISTRY_IMPL_H_
