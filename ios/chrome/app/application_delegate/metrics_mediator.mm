// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metrics_mediator.h"

#include <sys/sysctl.h>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "build/branding_buildflags.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/ios/features.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#import "ios/chrome/browser/net/connection_type_observer_bridge.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/common/app_group/app_group_metrics_mainapp.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The amount of time (in seconds) to wait for the user to start a new task.
const NSTimeInterval kFirstUserActionTimeout = 30.0;

// Returns time delta since app launch as retrieved from kernel info about
// the current process.
base::TimeDelta TimeDeltaSinceAppLaunchFromProcess() {
  struct kinfo_proc info;
  size_t length = sizeof(struct kinfo_proc);
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)getpid()};
  const int kr = sysctl(mib, base::size(mib), &info, &length, nullptr, 0);
  DCHECK_EQ(KERN_SUCCESS, kr);

  const struct timeval time = info.kp_proc.p_starttime;
  const NSTimeInterval time_since_1970 =
      time.tv_sec + (time.tv_usec / (double)USEC_PER_SEC);
  NSDate* date = [NSDate dateWithTimeIntervalSince1970:time_since_1970];
  return base::TimeDelta::FromSecondsD(-date.timeIntervalSinceNow);
}
}  // namespace

namespace metrics_mediator {
NSString* const kAppEnteredBackgroundDateKey = @"kAppEnteredBackgroundDate";
}  // namespace metrics_mediator_constants

using metrics_mediator::kAppEnteredBackgroundDateKey;

@interface MetricsMediator ()<CRConnectionTypeObserverBridge> {
  // Whether or not the crash reports present at startup have been processed to
  // determine if the last app lifetime ended in an OOM crash.
  BOOL _hasProcessedCrashReportsPresentAtStartup;

  // Observer for the connection type.  Contains a valid object only if the
  // metrics setting is set to wifi-only.
  std::unique_ptr<ConnectionTypeObserverBridge> connectionTypeObserverBridge_;
}

// Starts or stops metrics recording and/or uploading.
- (void)setMetricsEnabled:(BOOL)enabled withUploading:(BOOL)allowUploading;
// Sets variables needed by the app_group application to collect UMA data.
// Process the pending logs produced by extensions.
// Called on start (cold and warm) and UMA settings change to update the
// collecting settings in extensions.
- (void)setAppGroupMetricsEnabled:(BOOL)enabled;
// Processes crash reports present at startup.
- (void)processCrashReportsPresentAtStartup;
// Starts or stops crash recording and/or uploading.
- (void)setBreakpadEnabled:(BOOL)enabled withUploading:(BOOL)allowUploading;
// Starts or stops watching for wwan events.
- (void)setWatchWWANEnabled:(BOOL)enabled;
// Enable/disable transmission of accumulated logs and crash reports (dumps).
- (void)setReporting:(BOOL)enableReporting;
// Enable/Disable uploading crash reports.
- (void)setBreakpadUploadingEnabled:(BOOL)enableUploading;
// Returns YES if the metrics are enabled and the reporting is wifi-only.
- (BOOL)isMetricsReportingEnabledWifiOnly;
// Update metrics prefs on a permission (opt-in/out) change. When opting out,
// this clears various client ids. When opting in, this resets saving crash
// prefs, so as not to trigger upload of various stale data.
// Mirrors the function in metrics_reporting_state.cc.
- (void)updateMetricsPrefsOnPermissionChange:(BOOL)enabled;
// Logs the number of tabs with UMAHistogramCount100 and allows testing.
+ (void)recordNumTabAtStartup:(int)numTabs;
// Logs the number of tabs with UMAHistogramCount100 and allows testing.
+ (void)recordNumTabAtResume:(int)numTabs;

@end

@implementation MetricsMediator

#pragma mark - Public methods.

+ (void)logStartupDuration:(id<StartupInformation>)startupInformation {
  if (![startupInformation isColdStart])
    return;

  const base::TimeDelta startDuration =
      base::TimeTicks::Now() - [startupInformation appLaunchTime];

  const base::TimeDelta startDurationFromProcess =
      TimeDeltaSinceAppLaunchFromProcess();

  UMA_HISTOGRAM_TIMES("Startup.ColdStartFromProcessCreationTime",
                      startDurationFromProcess);

  if ([startupInformation startupParameters]) {
    UMA_HISTOGRAM_TIMES("Startup.ColdStartWithExternalURLTime", startDuration);
  } else {
    UMA_HISTOGRAM_TIMES("Startup.ColdStartWithoutExternalURLTime",
                        startDuration);
  }
}

+ (void)logDateInUserDefaults {
  [[NSUserDefaults standardUserDefaults]
      setObject:[NSDate date]
         forKey:metrics_mediator::kAppEnteredBackgroundDateKey];
}

+ (void)logLaunchMetricsWithStartupInformation:
            (id<StartupInformation>)startupInformation
                             interfaceProvider:(id<BrowserInterfaceProvider>)
                                                   interfaceProvider {
  int numTabs =
      static_cast<int>(interfaceProvider.mainInterface.tabModel.count);
  if (startupInformation.isColdStart) {
    [self recordNumTabAtStartup:numTabs];
  } else {
    [self recordNumTabAtResume:numTabs];
  }

  if (UIAccessibilityIsVoiceOverRunning()) {
    base::RecordAction(
        base::UserMetricsAction("MobileVoiceOverActiveOnLaunch"));
  }

  // Create the first user action recorder and schedule a task to expire it
  // after some timeout. If unable to determine the last time the app entered
  // the background (i.e. either first run or restore after crash), don't bother
  // recording the first user action since fresh start wouldn't be triggered.
  NSDate* lastAppClose = [[NSUserDefaults standardUserDefaults]
      objectForKey:kAppEnteredBackgroundDateKey];
  if (lastAppClose) {
    NSTimeInterval interval = -[lastAppClose timeIntervalSinceNow];
    [startupInformation
        activateFirstUserActionRecorderWithBackgroundTime:interval];

    web::WebState* currentWebState = interfaceProvider.currentInterface.tabModel
                                         .webStateList->GetActiveWebState();
    if (currentWebState &&
        currentWebState->GetLastCommittedURL() == kChromeUINewTabURL) {
      startupInformation.firstUserActionRecorder->RecordStartOnNTP();
      [startupInformation resetFirstUserActionRecorder];
    } else {
      [startupInformation
          expireFirstUserActionRecorderAfterDelay:kFirstUserActionTimeout];
    }
    // Remove the value so it's not reused if the app crashes.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kAppEnteredBackgroundDateKey];
  }
}

- (void)updateMetricsStateBasedOnPrefsUserTriggered:(BOOL)isUserTriggered {
  BOOL optIn = [self areMetricsEnabled];
  BOOL allowUploading = [self isUploadingEnabled];
  if (!base::FeatureList::IsEnabled(kUmaCellular)) {
    BOOL wifiOnly = GetApplicationContext()->GetLocalState()->GetBoolean(
        prefs::kMetricsReportingWifiOnly);
    optIn = optIn && wifiOnly;
  }

  if (isUserTriggered)
    [self updateMetricsPrefsOnPermissionChange:optIn];
  [self setMetricsEnabled:optIn withUploading:allowUploading];
  [self setBreakpadEnabled:optIn withUploading:allowUploading];
  [self setWatchWWANEnabled:optIn];
  [self setAppGroupMetricsEnabled:optIn];
}

- (BOOL)areMetricsEnabled {
// If this if-def changes, it needs to be changed in
// IOSChromeMainParts::IsMetricsReportingEnabled and settings_egtest.mm.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  BOOL optIn = GetApplicationContext()->GetLocalState()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled);
#else
  // If a startup crash has been requested, then pretend that metrics have been
  // enabled, so that the app will go into recovery mode.
  BOOL optIn = experimental_flags::IsStartupCrashEnabled();
#endif
  return optIn;
}

- (BOOL)isUploadingEnabled {
  BOOL optIn = [self areMetricsEnabled];
  if (base::FeatureList::IsEnabled(kUmaCellular)) {
    return optIn;
  }
  BOOL wifiOnly = GetApplicationContext()->GetLocalState()->GetBoolean(
      prefs::kMetricsReportingWifiOnly);
  BOOL allowUploading = optIn;
  if (optIn && wifiOnly) {
    BOOL usingWWAN = net::NetworkChangeNotifier::IsConnectionCellular(
        net::NetworkChangeNotifier::GetConnectionType());
    allowUploading = !usingWWAN;
  }
  return allowUploading;
}

#pragma mark - Internal methods.

- (void)setMetricsEnabled:(BOOL)enabled withUploading:(BOOL)allowUploading {
  metrics::MetricsService* metrics =
      GetApplicationContext()->GetMetricsService();
  DCHECK(metrics);
  if (!metrics)
    return;
  if (enabled) {
    if (!metrics->recording_active())
      metrics->Start();

    if (allowUploading)
      metrics->EnableReporting();
    else
      metrics->DisableReporting();
  } else {
    if (metrics->recording_active())
      metrics->Stop();
  }
}

- (void)setAppGroupMetricsEnabled:(BOOL)enabled {
  app_group::ProceduralBlockWithData callback;
  if (enabled) {
    PrefService* prefs = GetApplicationContext()->GetLocalState();
    NSString* brandCode =
        base::SysUTF8ToNSString(ios::GetChromeBrowserProvider()
                                    ->GetAppDistributionProvider()
                                    ->GetDistributionBrandCode());

    app_group::main_app::EnableMetrics(
        base::SysUTF8ToNSString(
            GetApplicationContext()->GetMetricsService()->GetClientId()),
        brandCode, prefs->GetInt64(metrics::prefs::kInstallDate),
        prefs->GetInt64(metrics::prefs::kMetricsReportingEnabledTimestamp));

    // If metrics are enabled, process the logs. Otherwise, just delete them.
    callback = ^(NSData* log_content) {
      std::string log(static_cast<const char*>([log_content bytes]),
                      static_cast<size_t>([log_content length]));
      base::PostTask(
          FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
            GetApplicationContext()->GetMetricsService()->PushExternalLog(log);
          }));
    };
  } else {
    app_group::main_app::DisableMetrics();
  }

  app_group::main_app::RecordWidgetUsage();
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&app_group::main_app::ProcessPendingLogs, callback));
}

- (void)processCrashReportsPresentAtStartup {
  _hasProcessedCrashReportsPresentAtStartup = YES;
}

- (void)setBreakpadEnabled:(BOOL)enabled withUploading:(BOOL)allowUploading {
  breakpad_helper::SetUserEnabledUploading(enabled);
  if (enabled) {
    breakpad_helper::SetEnabled(true);

    // Do some processing of the crash reports present at startup. Note that
    // this processing must be done before uploading is enabled because once
    // uploading starts the number of crash reports present will begin to
    // decrease as they are uploaded. The ordering is ensured here because both
    // the crash report processing and the upload enabling are handled by
    // posting blocks to a single |dispath_queue_t| in BreakpadController.
    if (!_hasProcessedCrashReportsPresentAtStartup && allowUploading) {
      [self processCrashReportsPresentAtStartup];
    }
    [self setBreakpadUploadingEnabled:(![[PreviousSessionInfo sharedInstance]
                                           isFirstSessionAfterUpgrade] &&
                                       allowUploading)];
  } else {
    breakpad_helper::SetEnabled(false);
  }
}

- (void)setWatchWWANEnabled:(BOOL)enabled {
  if (!enabled) {
    connectionTypeObserverBridge_.reset();
    return;
  }

  if (!connectionTypeObserverBridge_) {
    connectionTypeObserverBridge_.reset(new ConnectionTypeObserverBridge(self));
  }
}

- (void)updateMetricsPrefsOnPermissionChange:(BOOL)enabled {
  // TODO(crbug.com/635669): Consolidate with metrics_reporting_state.cc
  // function.
  metrics::MetricsService* metrics =
      GetApplicationContext()->GetMetricsService();
  DCHECK(metrics);
  if (!metrics)
    return;
  if (enabled) {
    // When a user opts in to the metrics reporting service, the previously
    // collected data should be cleared to ensure that nothing is reported
    // before a user opts in and all reported data is accurate.
    if (!metrics->recording_active())
      metrics->ClearSavedStabilityMetrics();
  } else {
    // Clear the client id pref when opting out.
    // Note: Clearing client id will not affect the running state (e.g. field
    // trial randomization), as the pref is only read on startup.
    GetApplicationContext()->GetLocalState()->ClearPref(
        metrics::prefs::kMetricsClientID);
    GetApplicationContext()->GetLocalState()->ClearPref(
        metrics::prefs::kMetricsReportingEnabledTimestamp);
    crash_keys::ClearMetricsClientId();
  }
}

+ (void)disableReporting {
  breakpad_helper::SetUploadingEnabled(false);
  metrics::MetricsService* metrics =
      GetApplicationContext()->GetMetricsService();
  DCHECK(metrics);
  metrics->DisableReporting();
}

+ (void)applicationDidEnterBackground:(NSInteger)memoryWarningCount {
  base::RecordAction(base::UserMetricsAction("MobileEnteredBackground"));
  UMA_HISTOGRAM_COUNTS_100("MemoryWarning.OccurrencesPerSession",
                           memoryWarningCount);
}

#pragma mark - CRConnectionTypeObserverBridge implementation

- (void)connectionTypeChanged:(net::NetworkChangeNotifier::ConnectionType)type {
  BOOL wwanEnabled = net::NetworkChangeNotifier::IsConnectionCellular(type);
  // Currently the MainController only cares about WWAN state for the metrics
  // reporting preference.  If it's disabled, or the wifi-only preference is
  // not set, we don't care.  In fact, we should not even be getting this call.
  DCHECK([self isMetricsReportingEnabledWifiOnly]);
  // |wwanEnabled| is true if a cellular connection such as EDGE or GPRS is
  // used. Otherwise, either there is no connection available, or another link
  // (such as WiFi) is used.
  if (wwanEnabled) {
    // If WWAN mode is on, wifi-only prefs should be disabled.
    // For the crash reporter, we still want to record the crashes.
    [self setBreakpadUploadingEnabled:NO];
    [self setReporting:NO];
  } else if ([self areMetricsEnabled]) {
    // Double-check that the metrics reporting preference is enabled.
    if (![[PreviousSessionInfo sharedInstance] isFirstSessionAfterUpgrade])
      [self setBreakpadUploadingEnabled:YES];
    [self setReporting:YES];
  }
}

#pragma mark - interfaces methods

+ (void)recordNumTabAtStartup:(int)numTabs {
  UMA_HISTOGRAM_COUNTS_100("Tabs.CountAtStartup", numTabs);
}

+ (void)recordNumTabAtResume:(int)numTabs {
  UMA_HISTOGRAM_COUNTS_100("Tabs.CountAtResume", numTabs);
}

- (void)setBreakpadUploadingEnabled:(BOOL)enableUploading {
  breakpad_helper::SetUploadingEnabled(enableUploading);
}

- (void)setReporting:(BOOL)enableReporting {
  if (enableReporting) {
    GetApplicationContext()->GetMetricsService()->EnableReporting();
  } else {
    GetApplicationContext()->GetMetricsService()->DisableReporting();
  }
}

- (BOOL)isMetricsReportingEnabledWifiOnly {
  BOOL optIn = GetApplicationContext()->GetLocalState()->GetBoolean(
      metrics::prefs::kMetricsReportingEnabled);
  if (base::FeatureList::IsEnabled(kUmaCellular)) {
    return optIn;
  }
  return optIn && GetApplicationContext()->GetLocalState()->GetBoolean(
                      prefs::kMetricsReportingWifiOnly);
}

@end
