// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>
#import <UserNotifications/UserNotifications.h>

#import <algorithm>

#import "base/barrier_callback.h"
#import "components/metrics/metrics_service.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_prepopulate_data.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/background_refresh_constants.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/content_notification/model/content_notification_util.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_profile_helper.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/model/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/model/feed_constants.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"

namespace {

// A variation of base::ScopedClosureRunner that invokes a callback with
// default arguments if the callback has not been invoked directly.
template <typename Signature>
class ScopedCallbackRunner;

template <typename R, typename... Args>
class ScopedCallbackRunner<R(Args...)> {
 public:
  [[nodiscard]] explicit ScopedCallbackRunner(
      base::OnceCallback<R(Args...)> callback,
      Args&&... args)
      : callback_(std::move(callback)), args_(std::forward<Args>(args)...) {}

  ScopedCallbackRunner(ScopedCallbackRunner&&) = default;
  ScopedCallbackRunner& operator=(ScopedCallbackRunner&&) = default;

  ~ScopedCallbackRunner() { RunAndReset(); }

  void RunAndReset() {
    RunAndReset(std::make_index_sequence<sizeof...(Args)>());
  }

  [[nodiscard]] base::OnceCallback<R(Args...)> Release() && {
    return std::move(callback_);
  }

 private:
  template <size_t... Indexes>
  void RunAndReset(std::index_sequence<Indexes...>) {
    if (callback_) {
      std::move(callback_).Run(std::get<Indexes>(std::move(args_))...);
    }
  }

  base::OnceCallback<R(Args...)> callback_;
  std::tuple<Args...> args_;
};

// Returns a callback that will invoke `callback` if called. If the returned
// callback is not invoked, then instead `callback` will be invoked with
// `default_args...`.
template <typename R, typename... Args>
base::OnceCallback<R(Args...)> EnsureCallbackCalled(
    base::OnceCallback<R(Args...)> callback,
    Args&&... default_args) {
  using Runner = ScopedCallbackRunner<R(Args...)>;
  return base::BindOnce(
      [](Runner runner, Args... args) -> R {
        return std::move(runner).Release().Run(std::forward<Args>(args)...);
      },
      Runner(std::move(callback), std::forward<Args>(default_args)...));
}

// Returns a callback that will invoke `callback` if called. If the returned
// callback is not invoked, then instead `callback` will be invoked with
// `default_args...`.
template <typename R, typename... Args>
base::OnceCallback<R(Args...)> EnsureCallbackCalled(
    base::RepeatingCallback<R(Args...)> callback,
    Args&&... default_args) {
  using Runner = ScopedCallbackRunner<R(Args...)>;
  return base::BindOnce(
      [](Runner runner, Args... args) -> R {
        return std::move(runner).Release().Run(std::forward<Args>(args)...);
      },
      Runner(std::move(callback), std::forward<Args>(default_args)...));
}

// Wraps a list of DiscoverFeedProfileHelper. This code is derived
// from CRBProtocolObservers (but specialized) as it cannot use this class as
// it needs to know the number of observers still present.
class DiscoverFeedProfileHelperList {
 public:
  DiscoverFeedProfileHelperList() = default;

  DiscoverFeedProfileHelperList(const DiscoverFeedProfileHelperList&) = delete;
  DiscoverFeedProfileHelperList& operator=(
      const DiscoverFeedProfileHelperList&) = delete;

  ~DiscoverFeedProfileHelperList() = default;

  // Adds `helper` to this list.
  void AddHelper(id<DiscoverFeedProfileHelper> helper) {
    auto iter = std::find(helpers_.begin(), helpers_.end(), helper);
    CHECK(iter == helpers_.end());
    helpers_.push_back(helper);
  }

  // Removes `helper` from this list.
  void RemoveHelper(id<DiscoverFeedProfileHelper> helper) {
    auto iter = std::find(helpers_.begin(), helpers_.end(), helper);
    CHECK(iter != helpers_.end());
    helpers_.erase(iter);
  }

  // Invokes -refreshFeedInBackground for all helpers.
  void RefreshFeedInBackground() {
    Compact();
    for (id<DiscoverFeedProfileHelper> helper : helpers_) {
      [helper refreshFeedInBackground];
    }
  }

 private:
  // Compacts the list of helpers, removing all nil weak pointers.
  void Compact() {
    helpers_.erase(std::remove(helpers_.begin(), helpers_.end(), nil),
                   helpers_.end());
  }

  std::vector<__weak id<DiscoverFeedProfileHelper>> helpers_;
};

}  // namespace

@implementation DiscoverFeedAppAgent {
  // Set to YES when the app is foregrounded.
  BOOL _wasForegroundedAtLeastOnce;
  DiscoverFeedProfileHelperList _helpers;
}

#pragma mark - Public

- (void)addHelper:(id<DiscoverFeedProfileHelper>)helper {
  _helpers.AddHelper(helper);
}

- (void)removeHelper:(id<DiscoverFeedProfileHelper>)helper {
  _helpers.RemoveHelper(helper);
}

#pragma mark - ObservingAppAgent

- (void)appDidEnterBackground {
  if (!IsAvoidFeedRefreshOnBackgroundEnabled()) {
      _helpers.RefreshFeedInBackground();
  }
}

@end
