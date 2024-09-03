// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/metrics_mediator.h"

#import <mach/mach.h>
#import <sys/sysctl.h>

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/crash/core/common/crash_keys.h"
#import "components/metrics/metrics_pref_names.h"
#import "components/metrics/metrics_service.h"
#import "components/metrics/metrics_switches.h"
#import "components/prefs/pref_service.h"
#import "components/previous_session_info/previous_session_info.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/ukm/ios/ukm_reporting_ios_util.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/metric_kit_subscriber.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/startup/ios_enable_sandbox_dump_buildflags.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/metrics/model/first_user_action_recorder.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/connection_information.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/metrics.h"
#import "ios/chrome/browser/widget_kit/model/features.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/app_group/app_group_metrics_mainapp.h"
#import "ios/chrome/common/credential_provider/constants.h"
#import "ios/public/provider/chrome/browser/app_distribution/app_distribution_api.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
#import "ios/chrome/browser/widget_kit/model/widget_metrics_util.h"  // nogncheck
#endif

@class AppState;

namespace {
// The key to a NSUserDefaults entry logging the number of times classes are
// loaded before a scene is attached.
NSString* const kAppStartupCounterKey = @"LoadTimePreferenceKey";

// The amount of time (in seconds) to wait for the user to start a new task.
const NSTimeInterval kFirstUserActionTimeout = 30.0;

// Enum values for Startup.IOSColdStartType histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class ColdStartType : int {
  // Regular cold start.
  kRegular = 0,
  // Cold start with FRE.
  kFirstRun = 1,
  // Cold start after a device restore.
  kAfterDeviceRestore = 2,
  // Cold start after a Chrome upgrade.
  kAfterChromeUpgrade = 3,
  // Cold start after a device restore and Chrome upgrade.
  kAfterDeviceRestoreAndChromeUpgrade = 4,
  // Unknown device restore.
  kUnknownDeviceRestore = 5,
  // Unknown device restore and Chrome upgrade.
  kUnknownDeviceRestoreAndChromeUpgrade = 6,
  kMaxValue = kUnknownDeviceRestoreAndChromeUpgrade,
};

// Enum representing the existing set of all open tabs age scenarios. Current
// values should not be renumbered. Please keep in sync with
// "IOSAllOpenTabsAge" in src/tools/metrics/histograms/enums.xml.
enum class TabsAgeGroup {
  kLessThanOneDay = 0,
  kOneToThreeDays = 1,
  kThreeToSevenDays = 2,
  kSevenToFourteenDays = 3,
  kFourteenToThirtyDays = 4,
  kMoreThanThirtyDays = 5,
  kMaxValue = kMoreThanThirtyDays,
};

#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
// Returns time delta since app launch as retrieved from kernel info about
// the current process.
base::TimeDelta TimeDeltaSinceAppLaunchFromProcess() {
  struct kinfo_proc info;
  size_t length = sizeof(struct kinfo_proc);
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, (int)getpid()};
  const int kr = sysctl(mib, std::size(mib), &info, &length, nullptr, 0);
  DCHECK_EQ(KERN_SUCCESS, kr);

  const struct timeval time = info.kp_proc.p_starttime;
  const NSTimeInterval time_since_1970 =
      time.tv_sec + (time.tv_usec / (double)USEC_PER_SEC);
  NSDate* date = [NSDate dateWithTimeIntervalSince1970:time_since_1970];
  return base::Seconds(-date.timeIntervalSinceNow);
}

void DumpEnvironment(id<StartupInformation> startup_information) {
  if (![[NSUserDefaults standardUserDefaults]
          boolForKey:@"EnableDumpEnvironment"]) {
    return;
  }
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                       NSUserDomainMask, YES);
  NSError* error = nil;
  NSString* document_directory = [paths objectAtIndex:0];
  NSString* environment_directory =
      [document_directory stringByAppendingPathComponent:@"environment"];
  if (![[NSFileManager defaultManager]
          fileExistsAtPath:environment_directory]) {
    [[NSFileManager defaultManager] createDirectoryAtPath:environment_directory
                              withIntermediateDirectories:NO
                                               attributes:nil
                                                    error:&error];
    if (error) {
      return;
    }
  }
  NSDate* now_date = [NSDate date];
  NSDateFormatter* formatter = [[NSDateFormatter alloc] init];
  formatter.dateFormat = @"yyyyMMdd-HHmmss";
  formatter.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];

  NSString* file_name = [formatter stringFromDate:now_date];

  NSDictionary* environment = [[NSProcessInfo processInfo] environment];
  base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta processStartToNowTime =
      TimeDeltaSinceAppLaunchFromProcess();
  const base::TimeDelta mainToNowTime =
      now - [startup_information appLaunchTime];
  const base::TimeDelta didFinishLaunchingToNowTime =
      now - [startup_information didFinishLaunchingTime];
  const base::TimeDelta sceneConnectionToNowTime =
      now - [startup_information firstSceneConnectionTime];

  NSDictionary* dict = @{
    @"environment" : environment,
    @"now" : file_name,
    @"processStartToNowTime" : @(processStartToNowTime.InMilliseconds()),
    @"mainToNowTime" : @(mainToNowTime.InMilliseconds()),
    @"didFinishLaunchingToNowTime" :
        @(didFinishLaunchingToNowTime.InMilliseconds()),
    @"sceneConnectionToNowTime" : @(sceneConnectionToNowTime.InMilliseconds()),
  };

  NSData* data =
      [NSJSONSerialization dataWithJSONObject:dict
                                      options:NSJSONWritingPrettyPrinted

                                        error:&error];
  if (error) {
    return;
  }

  NSString* file_path =
      [environment_directory stringByAppendingPathComponent:file_name];
  [data writeToFile:file_path atomically:YES];
}
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)

// Returns the associated setting from inactive tab preference value.
InactiveTabsThresholdSetting InactiveTabsSettingFromPreference(int preference) {
  switch (preference) {
    case -1:
      return InactiveTabsThresholdSetting::kNeverMove;
    case 0:
      return InactiveTabsThresholdSetting::kDefaultValue;
    case 7:
      return InactiveTabsThresholdSetting::kOneWeek;
    case 14:
      return InactiveTabsThresholdSetting::kTwoWeeks;
    case 21:
      return InactiveTabsThresholdSetting::kThreeWeeks;
    default:
      return InactiveTabsThresholdSetting::kUnknown;
  }
}

const char kHistogramPrefixIncludingMismatch[] = "IOS.IncludingMismatch.";
const char kHistogramPrefix[] = "IOS.";

// Returns the warm start histogram prefix based on whether or not the metrics
// were collected for the same current app version or a previous version.
std::string WarmStartHistogramPrefix(bool version_mismatch) {
  return version_mismatch ? kHistogramPrefixIncludingMismatch
                          : kHistogramPrefix;
}
}  // namespace

// A class to log the "load" count in uma.
@interface AppStartupCounter : NSObject
@end

@implementation AppStartupCounter
+ (void)load {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setInteger:[defaults integerForKey:kAppStartupCounterKey] + 1
                forKey:kAppStartupCounterKey];
}
@end

namespace metrics_mediator {
NSString* const kAppEnteredBackgroundDateKey = @"kAppEnteredBackgroundDate";
NSString* const kAppDidFinishLaunchingConsecutiveCallsKey =
    @"kAppDidFinishLaunchingConsecutiveCallsKey";

void RecordWidgetUsage(base::span<const HistogramNameCountPair> histograms) {
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
    app_group::kCredentialExtensionCopyUserDisplayNameCount :
        @"IOS.CredentialExtension.CopyUserDisplayNameCount",
    app_group::kCredentialExtensionCopyCreationDateCount :
        @"IOS.CredentialExtension.CopyCreationDateCount",
    app_group::kCredentialExtensionCopyPasswordCount :
        @"IOS.CredentialExtension.CopyPasswordCount",
    app_group::kCredentialExtensionShowPasswordCount :
        @"IOS.CredentialExtension.ShowPasswordCount",
    app_group::
    kCredentialExtensionSearchCount : @"IOS.CredentialExtension.SearchCount",
    app_group::kCredentialExtensionPasswordUseCount :
        @"IOS.CredentialExtension.PasswordUseCount",
    app_group::kCredentialExtensionPasskeyUseCount :
        @"IOS.CredentialExtension.PasskeyUseCount",
    app_group::kCredentialExtensionQuickPasswordUseCount :
        @"IOS.CredentialExtension.QuickPasswordUseCount",
    app_group::kCredentialExtensionQuickPasskeyUseCount :
        @"IOS.CredentialExtension.QuickPasskeyUseCount",
    app_group::kCredentialExtensionFetchPasswordFailureCount :
        @"IOS.CredentialExtension.FetchPasswordFailure",
    app_group::kCredentialExtensionFetchPasswordNilArgumentCount :
        @"IOS.CredentialExtension.FetchPasswordNilArgument",
    app_group::kCredentialExtensionKeychainSavePasswordFailureCount :
        @"IOS.CredentialExtension.KeychainSavePasswordFailureCount",
    app_group::kCredentialExtensionSaveCredentialFailureCount :
        @"IOS.CredentialExtension.SaveCredentialFailureCount",
  };

  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  for (NSString* key in keyMetric) {
    int count = [shared_defaults integerForKey:key];
    if (count != 0) {
      base::UmaHistogramCounts1000(SysNSStringToUTF8(keyMetric[key]), count);
      [shared_defaults setInteger:0 forKey:key];
    }
  }

  for (const HistogramNameCountPair& pair : histograms) {
    int maxSamples = pair.buckets;
    // Check each possible bucket to see if it has any events to emit.
    for (int bucket = 0; bucket < maxSamples; ++bucket) {
      NSString* key = app_group::HistogramCountKey(pair.name, bucket);
      int count = [shared_defaults integerForKey:key];
      if (count != 0) {
        [shared_defaults setInteger:0 forKey:key];
        std::string histogramName = SysNSStringToUTF8(pair.name);
        for (int emitCount = 0; emitCount < count; ++emitCount) {
          base::UmaHistogramExactLinear(histogramName, bucket, maxSamples + 1);
        }
      }
    }
  }
}
}  // namespace metrics_mediator

using metrics_mediator::kAppEnteredBackgroundDateKey;
using metrics_mediator::kAppDidFinishLaunchingConsecutiveCallsKey;

@interface MetricsMediator ()
// Starts or stops metrics recording.
- (void)setMetricsEnabled:(BOOL)enabled;
// Sets variables needed by the app_group application to collect UMA data.
// Process the pending logs produced by extensions.
// Called on start (cold and warm) and UMA settings change to update the
// collecting settings in extensions.
- (void)setAppGroupMetricsEnabled:(BOOL)enabled;
// Update metrics prefs on a permission (opt-in/out) change. When opting out,
// this clears various client ids. When opting in, this resets saving crash
// prefs, so as not to trigger upload of various stale data.
// Mirrors the function in metrics_reporting_state.cc.
- (void)updateMetricsPrefsOnPermissionChange:(BOOL)enabled;
// Logs the inactive tabs settings preference.
+ (void)recordInactiveTabsSettingsAtStartup:(int)preference;
// Logs the number of active tabs (based on the arm's definition of
// active/inactive).
+ (void)recordStartupActiveTabCount:(int)tabCount;
// Logs the number of inactive tabs (based on the arm's definition of
// active/inactive).
+ (void)recordStartupInactiveTabCount:(int)tabCount;
// Logs the number of tabs older than 21 days.
+ (void)recordStartupAbsoluteInactiveTabCount:(int)tabCount;
// Logs the number of pinned tabs at startup.
+ (void)recordStartupPinnedTabCount:(int)tabCount;
// Logs the number of tabs with UMAHistogramCount100 and allows testing.
+ (void)recordStartupTabCount:(int)tabCount;
// Logs the number of tab groups with UmaHistogramCounts1M.
+ (void)recordStartupTabGroupCount:(int)tabGroupCount;
// Logs the number of tabs per group with UMAHistogramCount10000.
+ (void)recordStartupTabsPerGroupCount:(int)tabsPerGroupCount;
// Logs the number of tabs with UMAHistogramCount100 and allows testing.
+ (void)recordResumeTabCount:(int)tabCount;
// Logs the number of NTP tabs with UMAHistogramCount100 and allows testing.
+ (void)recordStartupNTPTabCount:(int)tabCount;
// Logs the number of NTP tabs with UMAHistogramCount100 and allows testing.
+ (void)recordResumeNTPTabCount:(int)tabCount;
// Logs the number of live NTP tabs with UMAHistogramCount100 and allows
// testing.
+ (void)recordResumeLiveNTPTabCount:(int)tabCount;

// Logs the number of old (inactive for more than 7 days) tabs with
// UMAHistogramCount100 and allows testing.
+ (void)recordStartupOldTabCount:(int)tabCount;
// Logs the number of duplicated tabs with UMAHistogramCount100 and allows
// testing.
+ (void)recordStartupDuplicatedTabCount:(int)tabCount;
// Logs the age (time elapsed since creation) of each tab  and allows testing.
+ (void)recordTabsAgeAtStartup:(const std::vector<base::TimeDelta>&)tabsAge;
// Returns a corresponding TabAgeGroup for provided `timeSinceCreation` time.
+ (TabsAgeGroup)tabsAgeGroupFromTimeSinceCreation:
    (base::TimeDelta)timeSinceCreation;
// Logs the number of connected and disconnected scenes.
+ (void)recordConnectedAndDisconnectedSceneCount:(int)connectedScenes;
@end

@implementation MetricsMediator

// Indicates whether credential extension was used while chrome was inactive.
BOOL _credentialExtensionWasUsed = NO;

#pragma mark - Public methods.

+ (void)createStartupTrackingTask {
  [MetricKitSubscriber createExtendedLaunchTask];
}

+ (void)logStartupDuration:(id<StartupInformation>)startupInformation {
  if (![startupInformation isColdStart])
    return;

  [MetricKitSubscriber endExtendedLaunchTask];
  base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta mainToNowTime =
      now - [startupInformation appLaunchTime];
  const base::TimeDelta didFinishLaunchingToNowTime =
      now - [startupInformation didFinishLaunchingTime];
  const base::TimeDelta sceneConnectionToNowTime =
      now - [startupInformation firstSceneConnectionTime];

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  int consecutiveLoads = [defaults integerForKey:kAppStartupCounterKey];
  [defaults removeObjectForKey:kAppStartupCounterKey];
  int consecutiveDidFinishLaunching =
      [defaults integerForKey:kAppDidFinishLaunchingConsecutiveCallsKey];
  [defaults removeObjectForKey:kAppDidFinishLaunchingConsecutiveCallsKey];

  base::UmaHistogramTimes("Startup.ColdStartFromMain", mainToNowTime);
  base::UmaHistogramTimes("Startup.TimeFromMainToDidFinishLaunchingCall",
                          mainToNowTime - didFinishLaunchingToNowTime);
  base::UmaHistogramTimes("Startup.TimeFromMainToSceneConnection",
                          mainToNowTime - sceneConnectionToNowTime);
  base::UmaHistogramCounts100("Startup.ConsecutiveLoadsWithoutLaunch",
                              consecutiveLoads);
  base::UmaHistogramCounts100(
      "Startup.ConsecutiveDidFinishLaunchingWithoutLaunch",
      consecutiveDidFinishLaunching);
#if BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
  DumpEnvironment(startupInformation);
#endif  // BUILDFLAG(IOS_ENABLE_SANDBOX_DUMP)
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

  int tabCount = 0;
  int tabGroupCount = 0;
  int pinnedTabCount = 0;
  int NTPTabCount = 0;
  int liveNTPTabCount = 0;
  int oldTabCount = 0;
  int duplicatedTabCount = 0;
  int activeTabCount = 0;
  int inactiveTabCount = 0;
  int absoluteInactiveTabCount = 0;

  // Amount of time after which a tab is considered as old.
  constexpr base::TimeDelta kOldTabThreshold = base::Days(7);

  // Amount of time after which a tab is considered as absolutely inactive.
  constexpr base::TimeDelta kAbsoluteInactiveTabThreshold = base::Days(21);

  NSMutableSet* uniqueURLs = [NSMutableSet set];
  std::vector<base::TimeDelta> timesSinceCreation;
  const base::Time now = base::Time::Now();

  for (SceneState* scene in scenes) {
    if (!scene.browserProviderInterface) {
      // The scene might not yet be initiated.
      // TODO(crbug.com/40123792): This will not be an issue when the tabs are
      // counted in sessions instead of scenes.
      continue;
    }

    const WebStateList* webStateList =
        scene.browserProviderInterface.mainBrowserProvider.browser
            ->GetWebStateList();
    const WebStateList* inactiveWebStateList =
        scene.browserProviderInterface.mainBrowserProvider.inactiveBrowser
            ->GetWebStateList();
    const int webStateListCount = webStateList->count();
    const int inactiveWebStateListCount = inactiveWebStateList->count();

    for (const TabGroup* group : webStateList->GetGroups()) {
      tabGroupCount++;
      [self recordStartupTabsPerGroupCount:group->range().count()];
    }

    tabCount += webStateListCount + inactiveWebStateListCount;
    pinnedTabCount += webStateList->pinned_tabs_count();
    activeTabCount += webStateListCount;
    inactiveTabCount += inactiveWebStateListCount;
    // All inactive tabs are inactive since minimum 7 days or more.
    oldTabCount += inactiveWebStateListCount;

    for (int i = 0; i < webStateListCount; i++) {
      web::WebState* webState = webStateList->GetWebStateAt(i);
      const bool wasWebStateRealized = webState->IsRealized();
      const GURL& URL = webState->GetVisibleURL();

      // Count NTPs.
      if (IsURLNewTabPage(URL)) {
        NTPTabCount++;
      }

      // Count duplicate URLs.
      NSString* URLString = base::SysUTF8ToNSString(URL.GetWithoutRef().spec());
      if ([uniqueURLs containsObject:URLString]) {
        duplicatedTabCount++;
      } else {
        [uniqueURLs addObject:URLString];
      }

      // Count old tabs.
      base::TimeDelta inactiveTime = now - webState->GetLastActiveTime();
      if (inactiveTime > kOldTabThreshold) {
        oldTabCount++;
        // Count absolute inactive tabs.
        if (inactiveTime > kAbsoluteInactiveTabThreshold) {
          absoluteInactiveTabCount++;
        }
      }

      // Calculate the age (time elapsed since creation) of WebState.
      base::TimeDelta timeSinceCreation = now - webState->GetCreationTime();
      timesSinceCreation.push_back(timeSinceCreation);

      DCHECK_EQ(wasWebStateRealized, webState->IsRealized());
    }

    for (int i = 0; i < inactiveWebStateListCount; i++) {
      web::WebState* webState = inactiveWebStateList->GetWebStateAt(i);

      // Calculate the age (time elapsed since creation) of WebState.
      base::TimeDelta timeSinceCreation = now - webState->GetCreationTime();
      timesSinceCreation.push_back(timeSinceCreation);

      // Calculate absolute inactive tabs.
      base::TimeDelta inactiveTime =
          base::Time::Now() - webState->GetLastActiveTime();
      if (inactiveTime > kAbsoluteInactiveTabThreshold) {
        absoluteInactiveTabCount++;
      }
    }
  }

  if (startupInformation.isColdStart) {
    [self recordInactiveTabsSettingsAtStartup:
              GetApplicationContext()->GetLocalState()->GetInteger(
                  prefs::kInactiveTabsTimeThreshold)];
    [self recordStartupActiveTabCount:activeTabCount];
    [self recordStartupInactiveTabCount:inactiveTabCount];
    [self recordStartupAbsoluteInactiveTabCount:absoluteInactiveTabCount];
    [self recordStartupPinnedTabCount:pinnedTabCount];
    [self recordStartupTabCount:tabCount];
    [self recordStartupTabGroupCount:tabGroupCount];
    [self recordStartupNTPTabCount:NTPTabCount];
    [self recordStartupOldTabCount:oldTabCount];
    [self recordStartupDuplicatedTabCount:duplicatedTabCount];
    [self recordTabsAgeAtStartup:timesSinceCreation];
    [self recordAndResetWarmStartCount];
  } else {
    [[PreviousSessionInfo sharedInstance] incrementWarmStartCount];
    [self recordResumeTabCount:tabCount];
    [self recordResumeNTPTabCount:NTPTabCount];
    // Only log at resume since there are likely no live NTPs on startup.
    [self recordResumeLiveNTPTabCount:liveNTPTabCount];
  }

  [self recordConnectedAndDisconnectedSceneCount:scenes.count];

  if (UIAccessibilityIsVoiceOverRunning()) {
    base::RecordAction(
        base::UserMetricsAction("MobileVoiceOverActiveOnLaunch"));
  }

#if BUILDFLAG(ENABLE_WIDGET_KIT_EXTENSION)
  [WidgetMetricsUtil logInstalledWidgets];

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

    // Proceed if the active scene is initialized.
    if (activeScene.browserProviderInterface) {
      web::WebState* currentWebState =
          activeScene.browserProviderInterface.currentBrowserProvider.browser
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

  // Log browser cold start for default browser promo experiment stats.
  if (scenes.count != 0) {
    LogBrowserLaunched(startupInformation.isColdStart);
  }

  if (!startupInformation.isColdStart) {
    return;
  }
  signin::Tribool device_restore = IsFirstSessionAfterDeviceRestore();
  ColdStartType sessionType;
  if (startupInformation.isFirstRun) {
    sessionType = ColdStartType::kFirstRun;
  } else {
    bool afterUpgrade =
        [PreviousSessionInfo sharedInstance].isFirstSessionAfterUpgrade;
    switch (device_restore) {
      case signin::Tribool::kUnknown:
        sessionType = afterUpgrade
                          ? ColdStartType::kUnknownDeviceRestoreAndChromeUpgrade
                          : ColdStartType::kUnknownDeviceRestore;
        break;
      case signin::Tribool::kTrue:
        sessionType = afterUpgrade
                          ? ColdStartType::kAfterDeviceRestoreAndChromeUpgrade
                          : ColdStartType::kAfterDeviceRestore;
        break;
      case signin::Tribool::kFalse:
        sessionType = afterUpgrade ? ColdStartType::kAfterChromeUpgrade
                                   : ColdStartType::kRegular;
        break;
    }
  }
  base::UmaHistogramEnumeration("Startup.IOSColdStartType", sessionType);
}

- (void)updateMetricsStateBasedOnPrefsUserTriggered:(BOOL)isUserTriggered {
  BOOL optIn = [self areMetricsEnabled];
  if (isUserTriggered)
    [self updateMetricsPrefsOnPermissionChange:optIn];
  [self setMetricsEnabled:optIn];
  crash_helper::SetEnabled(optIn);
  [self setAppGroupMetricsEnabled:optIn];
  [[MetricKitSubscriber sharedInstance] setEnabled:optIn];
}

- (void)notifyCredentialProviderWasUsed:(feature_engagement::Tracker*)tracker {
  if (_credentialExtensionWasUsed) {
    default_browser::NotifyCredentialExtensionUsed(tracker);

    // Reset to avoid duplicate notifications.
    _credentialExtensionWasUsed = NO;
  }
}

- (BOOL)areMetricsEnabled {
  if (metrics::IsMetricsReportingForceEnabled()) {
    return YES;
  }
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

#pragma mark - Internal methods.

- (void)setMetricsEnabled:(BOOL)enabled {
  metrics::MetricsService* metrics =
      GetApplicationContext()->GetMetricsService();
  DCHECK(metrics);
  if (!metrics)
    return;
  if (enabled) {
    if (!metrics->recording_active())
      metrics->Start();

    metrics->EnableReporting();
  } else {
    if (metrics->recording_active())
      metrics->Stop();
  }
}

- (void)setAppGroupMetricsEnabled:(BOOL)enabled {
  if (enabled) {
    PrefService* prefs = GetApplicationContext()->GetLocalState();
    NSString* brandCode =
        base::SysUTF8ToNSString(ios::provider::GetBrandCode());

    app_group::main_app::EnableMetrics(
        base::SysUTF8ToNSString(
            GetApplicationContext()->GetMetricsService()->GetClientId()),
        brandCode, prefs->GetInt64(metrics::prefs::kInstallDate),
        prefs->GetInt64(metrics::prefs::kMetricsReportingEnabledTimestamp));

    // If metrics are enabled, process the logs. Otherwise, just delete them.
    // TODO(crbug.com/40548746): remove related code.
  } else {
    app_group::main_app::DisableMetrics();
  }

  // Save CPE use information before it gets reset in RecordWidgetUsage until we
  // are ready to log it into feture engagement tracker.
  [self saveCredentialExtensionWasUsed];

  // Histograms fired in extensions that need to be re-fired from the main app.
  const metrics_mediator::HistogramNameCountPair histogramsFromExtension[] = {
      {
          @"IOS.CredentialExtension.PasswordCreated",
          static_cast<int>(CPEPasswordCreated::kMaxValue) + 1,
      },
      {
          @"IOS.CredentialExtension.NewCredentialUsername",
          static_cast<int>(CPENewCredentialUsername::kMaxValue) + 1,
      }};
  metrics_mediator::RecordWidgetUsage(histogramsFromExtension);
}

- (void)updateMetricsPrefsOnPermissionChange:(BOOL)enabled {
  // TODO(crbug.com/41268699): Consolidate with metrics_reporting_state.cc
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
        metrics::prefs::kMetricsProvisionalClientID);
    GetApplicationContext()->GetLocalState()->ClearPref(
        metrics::prefs::kMetricsReportingEnabledTimestamp);
    crash_keys::ClearMetricsClientId();
  }
}

+ (void)applicationDidEnterBackground:(NSInteger)memoryWarningCount {
  base::RecordAction(base::UserMetricsAction("MobileEnteredBackground"));

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

- (void)saveCredentialExtensionWasUsed {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();

  int password_use_count = [shared_defaults
      integerForKey:app_group::kCredentialExtensionPasswordUseCount];
  int quick_password_use_count = [shared_defaults
      integerForKey:app_group::kCredentialExtensionQuickPasswordUseCount];
  int passkey_use_count = [shared_defaults
      integerForKey:app_group::kCredentialExtensionPasskeyUseCount];
  int quick_passkey_use_count = [shared_defaults
      integerForKey:app_group::kCredentialExtensionQuickPasskeyUseCount];
  if (password_use_count != 0 || quick_password_use_count != 0 ||
      passkey_use_count != 0 || quick_passkey_use_count != 0) {
    _credentialExtensionWasUsed = YES;
  }
}

#pragma mark - interfaces methods

+ (void)recordInactiveTabsSettingsAtStartup:(int)preference {
  UMA_HISTOGRAM_ENUMERATION(kInactiveTabsThresholdSettingHistogram,
                            InactiveTabsSettingFromPreference(preference));
}

+ (void)recordStartupActiveTabCount:(int)tabCount {
  base::UmaHistogramCounts100("Tabs.ActiveCountAtStartup", tabCount);
}

+ (void)recordStartupInactiveTabCount:(int)tabCount {
  base::UmaHistogramCounts100("Tabs.InactiveCountAtStartup", tabCount);
}

+ (void)recordStartupAbsoluteInactiveTabCount:(int)tabCount {
  base::UmaHistogramCounts1M("Tabs.OldCountAtStartup2", tabCount);
}

+ (void)recordStartupPinnedTabCount:(int)tabCount {
  base::UmaHistogramCounts1000("Tabs.PinnedCountAtStartup", tabCount);
}

+ (void)recordStartupTabCount:(int)tabCount {
  base::UmaHistogramCounts1M("Tabs.CountAtStartup2", tabCount);
}

+ (void)recordStartupTabGroupCount:(int)tabGroupCount {
  base::UmaHistogramCounts1M("TabGroups.CountAtStartup", tabGroupCount);
}

+ (void)recordStartupTabsPerGroupCount:(int)tabsPerGroupCount {
  base::UmaHistogramCounts10000("TabGroups.TabsPerGroupCountAtStartup",
                                tabsPerGroupCount);
}

+ (void)recordResumeTabCount:(int)tabCount {
  base::UmaHistogramCounts1M("Tabs.CountAtResume2", tabCount);
}

+ (void)recordStartupNTPTabCount:(int)tabCount {
  base::UmaHistogramCounts100("Tabs.NTPCountAtStartup", tabCount);
}

+ (void)recordResumeNTPTabCount:(int)tabCount {
  base::UmaHistogramCounts100("Tabs.NTPCountAtResume", tabCount);
}

+ (void)recordResumeLiveNTPTabCount:(int)tabCount {
  base::UmaHistogramCounts100("Tabs.LiveNTPCountAtResume", tabCount);
}

+ (void)recordStartupOldTabCount:(int)tabCount {
  base::UmaHistogramCounts1M("Tabs.UnusedCountAtStartup2", tabCount);
}

+ (void)recordStartupDuplicatedTabCount:(int)tabCount {
  base::UmaHistogramCounts100("Tabs.DuplicatesCountAtStartup", tabCount);
}

+ (void)recordTabsAgeAtStartup:(const std::vector<base::TimeDelta>&)tabsAge {
  for (const auto timeSinceCreation : tabsAge) {
    TabsAgeGroup tabsAgeGroup =
        [self tabsAgeGroupFromTimeSinceCreation:timeSinceCreation];
    UMA_HISTOGRAM_ENUMERATION("Tabs.TimeSinceCreationAtStartup", tabsAgeGroup);
  }
}

+ (void)recordConnectedAndDisconnectedSceneCount:(int)connectedScenes {
  base::UmaHistogramCounts100("IOS.MultiWindow.ConnectedScenesCount",
                              connectedScenes);
  base::UmaHistogramCounts100(
      "IOS.MultiWindow.DisconnectedScenesCount",
      UIApplication.sharedApplication.openSessions.count - connectedScenes);
}

+ (void)recordAndResetWarmStartCount {
  bool afterUpgrade =
      [PreviousSessionInfo sharedInstance].isFirstSessionAfterUpgrade;
  const std::string prefix = WarmStartHistogramPrefix(afterUpgrade);

  NSInteger warmStartCount =
      [PreviousSessionInfo sharedInstance].warmStartCount;
  base::UmaHistogramCounts100(prefix + "WarmStartCount", warmStartCount);
  // The total number of launches from the session is the number of warm starts,
  // plus the initial cold start.
  base::UmaHistogramCounts100(prefix + "AppLaunchesPerSession",
                              warmStartCount + 1);
  [[PreviousSessionInfo sharedInstance] resetWarmStartCount];
}

+ (TabsAgeGroup)tabsAgeGroupFromTimeSinceCreation:
    (base::TimeDelta)timeSinceCreation {
  if (timeSinceCreation < base::Days(1)) {
    return TabsAgeGroup::kLessThanOneDay;
  }

  if (timeSinceCreation < base::Days(3)) {
    return TabsAgeGroup::kOneToThreeDays;
  }

  if (timeSinceCreation < base::Days(7)) {
    return TabsAgeGroup::kThreeToSevenDays;
  }

  if (timeSinceCreation < base::Days(14)) {
    return TabsAgeGroup::kSevenToFourteenDays;
  }

  if (timeSinceCreation < base::Days(30)) {
    return TabsAgeGroup::kFourteenToThirtyDays;
  }

  return TabsAgeGroup::kMoreThanThirtyDays;
}

@end
