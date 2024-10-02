// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/discover_feed/model/discover_feed_app_agent.h"

#import <BackgroundTasks/BackgroundTasks.h>
#import <UserNotifications/UserNotifications.h>

#import "base/barrier_callback.h"
#import "base/ranges/algorithm.h"
#import "components/metrics/metrics_service.h"
#import "components/search_engines/prepopulated_engines.h"
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
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

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

// Returns whether `values` only contains `true`.
bool AllOperationSucceeded(std::vector<bool> values) {
  return base::ranges::all_of(values, std::identity());
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

  // Invokes -performBackgroundRefreshes for all helpers, invoking `callback`
  // with success if all the operation succeeded for all helpers.
  void PerformBackgroundRefreshes(BackgroundRefreshCallback callback) {
    Compact();

    auto barrier = base::BarrierCallback<bool>(
        helpers_.size(),
        base::BindOnce(&AllOperationSucceeded).Then(std::move(callback)));

    for (id<DiscoverFeedProfileHelper> helper : helpers_) {
      // Use `EnsureCallbackCallback(..., true)` to consider the operation was
      // a success if the Profile is destroyed while the operation is pending.
      [helper performBackgroundRefreshes:EnsureCallbackCalled(barrier, true)];
    }
  }

  // Invokes -handleBackgroundRefreshTaskExpiration for all helpers.
  void HandleBackgroundRefreshTaskExpiration() {
    Compact();
    for (id<DiscoverFeedProfileHelper> helper : helpers_) {
      [helper handleBackgroundRefreshTaskExpiration];
    }
  }

  // Invokes -earliestBackgroundRefreshDate for all helpers and returns the
  // smallest of all the dates.
  base::Time EarliestBackgroundRefreshDate() {
    Compact();

    base::Time result;
    for (id<DiscoverFeedProfileHelper> helper : helpers_) {
      const base::Time date = [helper earliestBackgroundRefreshDate];
      if (date != base::Time() && (result == base::Time() || date < result)) {
        result = date;
      }
    }
    return result;
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

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (appState.initStage == AppInitStage::kBrowserBasic) {
    // Apple docs say that background tasks must be registered before the
    // end of `application:didFinishLaunchingWithOptions:`.
    // AppInitStage::kBrowserBasic fulfills that requirement.
    [self maybeRegisterBackgroundRefreshTask];
    // This is a provisional permission, which does not prompt the user at this
    // point.
    [self maybeRequestUserNotificationPermissions];
  } else if (appState.initStage ==
             AppInitStage::kBrowserObjectsForBackgroundHandlers) {
    // Save the value of the feature flag now since 'base::FeatureList' was
    // not available in `AppInitStage::kBrowserBasic`.
    // IsFeedBackgroundRefreshCapabilityEnabled() simply reads the saved value
    // saved by SaveFeedBackgroundRefreshCapabilityEnabledForNextColdStart(). Do
    // not wrap this in IsFeedBackgroundRefreshCapabilityEnabled() -- in this
    // case, a new value would never be saved again once we save NO, since the
    // NO codepath would not execute saving a new value.
    SaveFeedBackgroundRefreshCapabilityEnabledForNextColdStart();
  }
  [super appState:appState didTransitionFromInitStage:previousInitStage];
}

#pragma mark - ObservingAppAgent

- (void)appDidEnterBackground {
  if (IsFeedBackgroundRefreshEnabled()) {
    [self scheduleBackgroundRefresh];
  } else {
    _helpers.RefreshFeedInBackground();
  }
}

- (void)appDidEnterForeground {
  _wasForegroundedAtLeastOnce = YES;
  if (IsFeedBackgroundRefreshCapabilityEnabled()) {
    // This is not strictly necessary, but it makes it more explicit. The OS
    // limits to 1 refresh task at any time, and a new request will replace a
    // previous request. Tasks are only executed in the background.
    [BGTaskScheduler.sharedScheduler cancelAllTaskRequests];
  }
}

#pragma mark - Helpers

// Registers handler for the background refresh task. According to
// documentation, this must complete before the end of
// `applicationDidFinishLaunching`.
- (void)maybeRegisterBackgroundRefreshTask {
  if (!IsFeedBackgroundRefreshCapabilityEnabled()) {
    return;
  }
  __weak DiscoverFeedAppAgent* weakSelf = self;
  [BGTaskScheduler.sharedScheduler
      registerForTaskWithIdentifier:kFeedBackgroundRefreshTaskIdentifier
                         usingQueue:nil
                      launchHandler:^(BGTask* task) {
                        dispatch_async(dispatch_get_main_queue(), ^{
                          [weakSelf handleBackgroundRefreshTask:task];
                        });
                      }];
}

// Schedules a background refresh task with an earliest begin date in the
// future. The OS limits to 1 refresh task at any time, and a new request will
// replace a previous request. Tasks are only executed in the background.
// TODO(crbug.com/40231475): It is critically important that we do not schedule
// other background fetch tasks (e.g., with other identifiers) anywhere,
// including other files. The OS only allows one fetch task at a time.
// Eventually, background fetches should be managed by a central manager.
- (void)scheduleBackgroundRefresh {
  // Do not DCHECK whether background refreshes were enabled at startup because
  // this is also called from the background task handler, and the value could
  // have changed during a cold start.
  if (!IsFeedBackgroundRefreshEnabled()) {
    return;
  }
  BGAppRefreshTaskRequest* request = [[BGAppRefreshTaskRequest alloc]
      initWithIdentifier:kFeedBackgroundRefreshTaskIdentifier];
  request.earliestBeginDate = [self earliestBackgroundRefreshBeginDate];
  // Error in scheduling is intentionally not handled since the fallback is that
  // the user will just refresh in the foreground.
  // TODO(crbug.com/40231475): Consider logging error in histogram.
  [BGTaskScheduler.sharedScheduler submitTaskRequest:request error:nil];
}

// Returns the earliest begin date to set on the refresh task. Either returns a
// date from DiscoverFeedService or an override date created with the override
// interval in Experimental Settings.
- (NSDate*)earliestBackgroundRefreshBeginDate {
  if (IsFeedOverrideDefaultsEnabled()) {
    return [NSDate
        dateWithTimeIntervalSinceNow:GetBackgroundRefreshIntervalInSeconds()];
  }

  const base::Time date = _helpers.EarliestBackgroundRefreshDate();
  return date != base::Time() ? date.ToNSDate() : nil;
}

// This method is called when the app is in the background.
- (void)handleBackgroundRefreshTask:(BGTask*)task {
  // Do not DCHECK whether background refreshes were enabled at startup because
  // the value could have changed during a cold start.
  if (!IsFeedBackgroundRefreshEnabled()) {
    return;
  }

  // TODO(crbug.com/40249480): Kill the app if in a cold start because currently
  // there are issues with background cold starts.
  if (!_wasForegroundedAtLeastOnce) {
    [self handleColdStartAndKillApp];
  }
  if (IsRecurringBackgroundRefreshScheduleEnabled()) {
    [self scheduleBackgroundRefresh];
  }
  task.expirationHandler = ^{
    dispatch_async(dispatch_get_main_queue(), ^{
      [self handleBackgroundRefreshTaskExpiration];
    });
  };

  // Cold starts are killed earlier in this method, so warm and cold starts
  // cannot be recorded at the same time.
  [self recordWarmStartMetrics];
  _helpers.PerformBackgroundRefreshes(base::BindOnce(^(bool success) {
    [self handleBackgroundRefreshCompletion:success task:task];
  }));
}

- (void)handleBackgroundRefreshTaskExpiration {
  _helpers.HandleBackgroundRefreshTaskExpiration();
  [self maybeNotifyRefreshSuccess:NO];
}

- (void)handleBackgroundRefreshCompletion:(bool)success task:(BGTask*)task {
  [self maybeNotifyRefreshSuccess:success];
  [task setTaskCompletedWithSuccess:success];
}

// Records cold start histogram and kills app.
- (void)handleColdStartAndKillApp {
  [FeedMetricsRecorder
      recordFeedRefreshTrigger:FeedRefreshTrigger::kBackgroundColdStart];

  // TODO(crbug.com/40249480): Remove this workaround and enable background
  // cold starts.
  [self maybeNotifyRefreshSuccess:NO];
  GetApplicationContext()->GetMetricsService()->OnAppEnterBackground();
  exit(0);
}

// Record refresh trigger for warm start.
- (void)recordWarmStartMetrics {
  [FeedMetricsRecorder
      recordFeedRefreshTrigger:FeedRefreshTrigger::kBackgroundWarmStart];
}

#pragma mark - Refresh Completion Notifications (only enabled by Experimental Settings)

// Request provisional permission, which does not explicitly prompt the user for
// permission. Instead, the OS delivers provisional notifications quietly and
// are visible in the notification center's history. For active debugging, the
// tester can go to the Settings App and turn on full permissions for banners
// and sounds.
- (void)maybeRequestUserNotificationPermissions {
  if (!IsFeedBackgroundRefreshCompletedNotificationEnabled()) {
    return;
  }

  [PushNotificationUtil enableProvisionalPushNotificationPermission:nil];
}

// Requests OS to send a local user notification with `title`.
- (void)maybeRequestNotification:(NSString*)title {
  if (!IsFeedBackgroundRefreshCompletedNotificationEnabled()) {
    return;
  }
  UNMutableNotificationContent* content =
      [[UNMutableNotificationContent alloc] init];
  content.title = title;
  content.body = @"This is enabled via Experimental Settings which is not "
                 @"available in stable.";
  UNTimeIntervalNotificationTrigger* trigger =
      [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:(1)
                                                         repeats:NO];
  UNNotificationRequest* request =
      [UNNotificationRequest requestWithIdentifier:[[NSUUID UUID] UUIDString]
                                           content:content
                                           trigger:trigger];
  UNUserNotificationCenter* center =
      UNUserNotificationCenter.currentNotificationCenter;
  [center addNotificationRequest:request withCompletionHandler:nil];
}

// Requests OS to send a local user notification that a feed refresh has been
// attempted in the background. The notification title says 'success' or
// 'failure' based on `success`.
- (void)maybeNotifyRefreshSuccess:(BOOL)success {
  NSString* title = nil;
  if (success) {
    title = @"Feed Bg Refresh Success";
  } else {
    title = @"Feed Bg Refresh Failure";
  }
  [self maybeRequestNotification:title];
  SetFeedRefreshTimestamp([NSDate now], kFeedLastBackgroundRefreshTimestamp);
}

@end
