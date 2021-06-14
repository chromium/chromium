// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metrics_mediator.h"

#include <sys/sysctl.h>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "build/branding_buildflags.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#include "components/ukm/ios/features.h"
#include "components/ukm/ios/ukm_reporting_ios_util.h"
#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/crash_report/crash_helper.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/metrics/first_user_action_recorder.h"
#import "ios/chrome/browser/net/connection_type_observer_bridge.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/connection_information.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/widget_kit/features.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"
#include "ios/chrome/common/app_group/app_group_metrics_mainapp.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
#import "ios/chrome/browser/widget_kit/widget_metrics_util.h"  // nogncheck
#endif

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

// Send histograms reporting the usage of notification center metrics.
void RecordWidgetUsage() {
  using base::SysNSStringToUTF8;

  // Dictionary containing the respective metric for each NSUserDefault's key.
  NSDictionary<NSString*, NSString*>* keyMetric = @{
    app_group::
    kContentExtensionDisplayCount : @"IOS.ContentExtension.DisplayCount",
    app_group::
    kSearchExtensionDisplayCount : @"IOS.SearchExtension.DisplayCount",
    app_group::
    kCredentialExtensionDisplayCount : @"IOS.CredentialExtension.DisplayCount",
    app_group::
    kCredentialExtensionReauthCount : @"IOS.CredentialExtension.ReauthCount",
    app_group::
    kCredentialExtensionCopyURLCount : @"IOS.CredentialExtension.CopyURLCount",
    app_group::kCredentialExtensionCopyUsernameCount :
        @"IOS.CredentialExtension.CopyUsernameCount",
    app_group::kCredentialExtensionCopyPasswordCount :
        @"IOS.CredentialExtension.CopyPasswordCount",
    app_group::kCredentialExtensionShowPasswordCount :
        @"IOS.CredentialExtension.ShowPasswordCount",
    app_group::
    kCredentialExtensionSearchCount : @"IOS.CredentialExtension.SearchCount",
    app_group::kCredentialExtensionPasswordUseCount :
        @"IOS.CredentialExtension.PasswordUseCount",
    app_group::kCredentialExtensionQuickPasswordUseCount :
        @"IOS.CredentialExtension.QuickPasswordUseCount",
    app_group::kCredentialExtensionFetchPasswordFailureCount :
        @"IOS.CredentialExtension.FetchPasswordFailure",
    app_group::kCredentialExtensionFetchPasswordNilArgumentCount :
        @"IOS.CredentialExtension.FetchPasswordNilArgument",
  };

  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  for (NSString* key in keyMetric) {
    int count = [shared_defaults integerForKey:key];
    if (count != 0) {
      base::UmaHistogramCounts1000(SysNSStringToUTF8(keyMetric[key]), count);
      [shared_defaults setInteger:0 forKey:key];
      if ([key isEqual:app_group::kCredentialExtensionPasswordUseCount] ||
          [key isEqual:app_group::kCredentialExtensionQuickPasswordUseCount]) {
        LogLikelyInterestedDefaultBrowserUserActivity(
            DefaultPromoTypeMadeForIOS);
      }
    }
  }
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
  std::unique_ptr<ConnectionTypeObserverBridge> _connectionTypeObserverBridge;
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
// Logs the number of NTP tabs with UMAHistogramCount100 and allows testing.
+ (void)recordNumNTPTabAtStartup:(int)numTabs;
// Logs the number of NTP tabs with UMAHistogramCount100 and allows testing.
+ (void)recordNumNTPTabAtResume:(int)numTabs;

@end

@implementation MetricsMediator

#pragma mark - Public methods.

+ (void)logStartupDuration:(id<StartupInformation>)startupInformation
     connectionInformation:(id<ConnectionInformation>)connectionInformation {
  if (![startupInformation isColdStart])
    return;

  const base::TimeDelta startDuration =
      base::TimeTicks::Now() - [startupInformation appLaunchTime];

  const base::TimeDelta startDurationFromProcess =
      TimeDeltaSinceAppLaunchFromProcess();

  base::UmaHistogramTimes("Startup.ColdStartFromProcessCreationTimeV2",
                          startDurationFromProcess);

  if ([connectionInformation startupParameters]) {
    base::UmaHistogramTimes("Startup.ColdStartWithExternalURLTime",
                            startDuration);
  } else {
    base::UmaHistogramTimes("Startup.ColdStartWithoutExternalURLTime",
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
                               connectedScenes:(NSArray<SceneState*>*)scenes {
  RecordAndResetUkmLogSizeOnSuccessCounter();

  int numTabs = 0;
  int numNTPTabs = 0;
  for (SceneState* scene in scenes) {
    if (!scene.interfaceProvider) {
      // The scene might not yet be initiated.
      // TODO(crbug.com/1064611): This will not be an issue when the tabs are
      // counted in sessions instead of scenes.
      continue;
    }

    const WebStateList* web_state_list =
        scene.interfaceProvider.mainInterface.browser->GetWebStateList();
    numTabs += web_state_list->count();
    for (int i = 0; i < web_state_list->count(); i++) {
      if (IsURLNewTabPage(web_state_list->GetWebStateAt(i)->GetVisibleURL())) {
        numNTPTabs++;
      }
    }
  }

  if (startupInformation.isColdStart) {
    [self recordNumTabAtStartup:numTabs];
    [self recordNumNTPTabAtStartup:numNTPTabs];
  } else {
    [self recordNumTabAtResume:numTabs];
    [self recordNumNTPTabAtResume:numNTPTabs];
  }

  if (UIAccessibilityIsVoiceOverRunning()) {
    base::RecordAction(
        base::UserMetricsAction("MobileVoiceOverActiveOnLaunch"));
  }

#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
  if (@available(iOS 14, *)) {
    [WidgetMetricsUtil logInstalledWidgets];
  }
#endif

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

    SceneState* activeScene = nil;
    for (SceneState* scene in scenes) {
      if (scene.activationLevel == SceneActivationLevelForegroundActive) {
        activeScene = scene;
        break;
      }
    }

    if (activeScene) {
      web::WebState* currentWebState =
          activeScene.interfaceProvider.currentInterface.browser
              ->GetWebStateList()
              ->GetActiveWebState();
      if (currentWebState &&
          currentWebState->GetLastCommittedURL() == kChromeUINewTabURL) {
        startupInformation.firstUserActionRecorder->RecordStartOnNTP();
        [startupInformation resetFirstUserActionRecorder];
      } else {
        [startupInformation
            expireFirstUserActionRecorderAfterDelay:kFirstUserActionTimeout];
      }
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
  if (@available(iOS 13, *)) {
    [[MetricKitSubscriber sharedInstance] setEnabled:optIn];
  }
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
    // TODO(crbug.com/782685): remove related code.
  } else {
    app_group::main_app::DisableMetrics();
  }
  RecordWidgetUsage();
}

- (void)processCrashReportsPresentAtStartup {
  _hasProcessedCrashReportsPresentAtStartup = YES;
}

- (void)setBreakpadEnabled:(BOOL)enabled withUploading:(BOOL)allowUploading {
  crash_helper::SetUserEnabledUploading(enabled);
  if (enabled) {
    crash_helper::SetEnabled(true);

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
    crash_helper::SetEnabled(false);
  }
}

- (void)setWatchWWANEnabled:(BOOL)enabled {
  if (!enabled) {
    _connectionTypeObserverBridge.reset();
    return;
  }

  if (!_connectionTypeObserverBridge) {
    _connectionTypeObserverBridge.reset(new ConnectionTypeObserverBridge(self));
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
  crash_helper::SetUploadingEnabled(false);
  metrics::MetricsService* metrics =
      GetApplicationContext()->GetMetricsService();
  DCHECK(metrics);
  metrics->DisableReporting();
}

+ (void)applicationDidEnterBackground:(NSInteger)memoryWarningCount {
  base::RecordAction(base::UserMetricsAction("MobileEnteredBackground"));
  base::UmaHistogramCounts100("MemoryWarning.OccurrencesPerSession",
                              memoryWarningCount);

  task_vm_info task_info_data;
  mach_msg_type_number_t count = sizeof(task_vm_info) / sizeof(natural_t);
  kern_return_t result =
      task_info(mach_task_self(), TASK_VM_INFO,
                reinterpret_cast<task_info_t>(&task_info_data), &count);
  if (result == KERN_SUCCESS) {
    mach_vm_size_t footprint_mb = task_info_data.phys_footprint / 1024 / 1024;
    base::UmaHistogramMemoryLargeMB(
        "Memory.Browser.MemoryFootprint.OnBackground", footprint_mb);
  }
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
  base::UmaHistogramCounts100("Tabs.CountAtStartup", numTabs);
}

+ (void)recordNumTabAtResume:(int)numTabs {
  base::UmaHistogramCounts100("Tabs.CountAtResume", numTabs);
}

+ (void)recordNumNTPTabAtStartup:(int)numTabs {
  base::UmaHistogramCounts100("Tabs.NTPCountAtStartup", numTabs);
}

+ (void)recordNumNTPTabAtResume:(int)numTabs {
  base::UmaHistogramCounts100("Tabs.NTPCountAtResume", numTabs);
}

- (void)setBreakpadUploadingEnabled:(BOOL)enableUploading {
  crash_helper::SetUploadingEnabled(enableUploading);
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
