// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_METRICS_MEDIATOR_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_METRICS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#include "base/containers/span.h"

@class SceneState;
@protocol StartupInformation;
namespace feature_engagement {
class Tracker;
}

namespace metrics_mediator {
// Key in the UserDefaults to store the date/time that the background fetch
// handler was called.
extern NSString* const kAppEnteredBackgroundDateKey;

// The key to a NSUserDefaults entry logging the number of times application
// didFinishLaunching is called before a scene is attached.
extern NSString* const kAppDidFinishLaunchingConsecutiveCallsKey;

// Struct containing histogram names and number of buckets. Used for recording
// histograms fired in extensions.
struct HistogramNameCountPair {
  NSString* name;
  int buckets;
};

// Send histograms reporting the usage of widget metrics. Uses the provided list
// of histogram names to see if any histograms have been logged in widgets.
void RecordWidgetUsage(base::span<const HistogramNameCountPair> histograms);

}  // namespace metrics_mediator

// Deals with metrics, checking and updating them accordingly to to the user
// preferences.
@interface MetricsMediator : NSObject
// Returns YES if the metrics pref is enabled.  Does not take into account the
// wifi-only option or wwan state.
- (BOOL)areMetricsEnabled;
// Starts or stops the metrics service and crash report recording and/or
// uploading, based on the current user preferences. Must be
// called both on initialization and after user triggered preference change.
// `isUserTriggered` is used to distinguish between those cases.
- (void)updateMetricsStateBasedOnPrefsUserTriggered:(BOOL)isUserTriggered;
// Logs the duration of the cold start startup. Does nothing if there isn't a
// cold start.
+ (void)logStartupDuration:(id<StartupInformation>)startupInformation;
// Creates a MetricKit extended launch task to track startup duration. This must
// be called before the first scene becomes active.
+ (void)createStartupTrackingTask;
// Logs the number of tabs open and the start type.
+ (void)logLaunchMetricsWithStartupInformation:
            (id<StartupInformation>)startupInformation
                               connectedScenes:(NSArray<SceneState*>*)scenes;
// Logs in UserDefaults the current date with kAppEnteredBackgroundDateKey as
// key.
+ (void)logDateInUserDefaults;
// Logs that the application is in background and the number of memory warnings
// for this session.
+ (void)applicationDidEnterBackground:(NSInteger)memoryWarningCount;

- (void)notifyCredentialProviderWasUsed:(feature_engagement::Tracker*)tracker;
@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_METRICS_MEDIATOR_H_
