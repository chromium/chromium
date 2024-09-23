// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/app_group/app_group_constants.h"

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/common/app_group/app_group_helper.h"
#import "ios/chrome/common/ios_app_bundle_id_prefix_buildflags.h"

namespace app_group {

extern NSString* const kChromeCapabilitiesPreference = @"Chrome.Capabilities";

extern NSString* const kChromeShowDefaultBrowserPromoCapability =
    @"ShowDefaultBrowserPromo";

const char kChromeAppGroupXCallbackCommand[] = "app-group-command";

NSString* const kChromeExtensionFieldTrialPreference = @"Extension.FieldTrial";

const char kChromeAppGroupCommandPreference[] =
    "GroupApp.ChromeAppGroupCommand";

const char kChromeAppGroupCommandTimePreference[] = "CommandTime";
const char kChromeAppGroupCommandAppPreference[] = "SourceApp";
const char kChromeAppGroupCommandCommandPreference[] = "Command";
const char kChromeAppGroupCommandTextPreference[] = "Text";
const char kChromeAppGroupCommandDataPreference[] = "Data";
const char kChromeAppGroupCommandIndexPreference[] = "Index";

const char kChromeAppGroupOpenURLCommand[] = "openurl";
const char kChromeAppGroupSearchTextCommand[] = "searchtext";
const char kChromeAppGroupSearchImageCommand[] = "searchimage";
const char kChromeAppGroupVoiceSearchCommand[] = "voicesearch";
const char kChromeAppGroupNewTabCommand[] = "newtab";
const char kChromeAppGroupFocusOmniboxCommand[] = "focusomnibox";
const char kChromeAppGroupIncognitoSearchCommand[] = "incognitosearch";
const char kChromeAppGroupQRScannerCommand[] = "qrscanner";
const char kChromeAppGroupLensCommand[] = "lens";
const char kChromeAppGroupSearchPasswordsCommand[] = "searchpasswords";

const char kChromeAppGroupSupportsSearchByImage[] = "supportsSearchByImage";
const char kChromeAppGroupIsGoogleDefaultSearchEngine[] =
    "isGoogleDefaultSearchEngine";
const char kChromeAppGroupEnableLensInWidget[] = "enableLensInWidget";
const char kChromeAppGroupEnableColorLensAndVoiceIconsInWidget[] =
    "enableColorLensAndVoiceIconsInWidget";

const char kChromeAppClientID[] = "ClientID";
const char kUserMetricsEnabledDate[] = "UserMetricsEnabledDate";
const char kInstallDate[] = "InstallDate";
const char kBrandCode[] = "BrandCode";

NSString* const kShareItemSource = @"Source";
NSString* const kShareItemURL = @"URL";
NSString* const kShareItemTitle = @"Title";
NSString* const kShareItemDate = @"Date";
NSString* const kShareItemCancel = @"Cancel";
NSString* const kShareItemType = @"Type";

NSString* const kShareItemSourceShareExtension = @"ChromeShareExtension";

NSString* const kOpenCommandSourceTodayExtension = @"ChromeTodayExtension";
NSString* const kOpenCommandSourceContentExtension = @"ChromeContentExtension";
NSString* const kOpenCommandSourceSearchExtension = @"ChromeSearchExtension";
NSString* const kOpenCommandSourceShareExtension = @"ChromeShareExtension";
NSString* const kOpenCommandSourceCredentialsExtension =
    @"ChromeCredentialsExtension";
NSString* const kOpenCommandSourceOpenExtension = @"ChromeOpenExtension";

NSString* const kSuggestedItems = @"SuggestedItems";

NSString* const kSuggestedItemsLastModificationDate =
    @"SuggestedItemsLastModificationDate";

NSString* const kOpenExtensionOutcomes = @"ChromeOpenExtensionOutcomes";

NSString* const kOpenExtensionOutcomeSuccess = @"OpenExtensionOutcomeSuccess";
NSString* const kOpenExtensionOutcomeFailureInvalidURL =
    @"OpenExtensionOutcomeFailureInvalidURL";
NSString* const kOpenExtensionOutcomeFailureURLNotFound =
    @"OpenExtensionOutcomeFailureURLNotFound";
NSString* const kOpenExtensionOutcomeFailureOpenInNotFound =
    @"OpenExtensionOutcomeFailureOpenInNotFound";
NSString* const kOpenExtensionOutcomeFailureUnsupportedScheme =
    @"OpenExtensionOutcomeFailureUnsupportedScheme";

NSString* ApplicationGroup() {
  return [AppGroupHelper applicationGroup];
}

NSString* CommonApplicationGroup() {
  NSBundle* bundle = base::apple::FrameworkBundle();
  NSString* group =
      [bundle objectForInfoDictionaryKey:@"KSCommonApplicationGroup"];
  if (![group length]) {
    return [NSString stringWithFormat:@"group.%s.common",
                                      BUILDFLAG(IOS_APP_BUNDLE_ID_PREFIX), nil];
  }
  return group;
}

NSString* ApplicationName(AppGroupApplications application) {
  switch (application) {
    case APP_GROUP_CHROME:
      return base::SysUTF8ToNSString(version_info::GetProductName());
    case APP_GROUP_TODAY_EXTENSION:
      return @"TodayExtension";
  }
}

NSUserDefaults* GetCommonGroupUserDefaults() {
  NSString* applicationGroup = CommonApplicationGroup();
  if (applicationGroup) {
    NSUserDefaults* defaults =
        [[NSUserDefaults alloc] initWithSuiteName:applicationGroup];
    if (defaults)
      return defaults;
  }

  // On a device, the entitlements should always provide an application group to
  // the application. This is not the case on simulator.
  DCHECK(TARGET_IPHONE_SIMULATOR);
  return [NSUserDefaults standardUserDefaults];
}

NSUserDefaults* GetGroupUserDefaults() {
  return [AppGroupHelper groupUserDefaults];
}

NSURL* LegacyShareExtensionItemsFolder() {
  NSURL* groupURL = [[NSFileManager defaultManager]
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* readingListURL =
      [groupURL URLByAppendingPathComponent:@"ShareExtensionItems"
                                isDirectory:YES];
  return readingListURL;
}

NSURL* ExternalCommandsItemsFolder() {
  NSURL* groupURL = [[NSFileManager defaultManager]
      containerURLForSecurityApplicationGroupIdentifier:
          CommonApplicationGroup()];
  NSURL* chromeURL =
      [groupURL URLByAppendingPathComponent:@"Chrome" isDirectory:YES];
  NSURL* externalCommandsURL =
      [chromeURL URLByAppendingPathComponent:@"ExternalCommands"
                                 isDirectory:YES];
  return externalCommandsURL;
}

NSURL* ContentWidgetFaviconsFolder() {
  return [AppGroupHelper widgetsFaviconsFolder];
}

NSURL* SharedFaviconAttributesFolder() {
  NSURL* groupURL = [[NSFileManager defaultManager]
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* chromeURL = [groupURL URLByAppendingPathComponent:@"Chrome"
                                               isDirectory:YES];
  NSURL* sharedFaviconAttributesURL =
      [chromeURL URLByAppendingPathComponent:@"SharedFaviconAttributes"
                                 isDirectory:YES];
  return sharedFaviconAttributesURL;
}

NSURL* CrashpadFolder() {
  NSURL* groupURL = [[NSFileManager defaultManager]
      containerURLForSecurityApplicationGroupIdentifier:ApplicationGroup()];
  NSURL* chromeURL = [groupURL URLByAppendingPathComponent:@"Chrome"
                                               isDirectory:YES];
  NSURL* crashpadURL = [chromeURL URLByAppendingPathComponent:@"Crashpad"
                                                  isDirectory:YES];
  return crashpadURL;
}

NSString* KeyForOpenExtensionOutcomeType(OpenExtensionOutcome type) {
  switch (type) {
    case OpenExtensionOutcome::kSuccess:
      return kOpenExtensionOutcomeSuccess;
    case OpenExtensionOutcome::kFailureInvalidURL:
      return kOpenExtensionOutcomeFailureInvalidURL;
    case OpenExtensionOutcome::kFailureURLNotFound:
      return kOpenExtensionOutcomeFailureURLNotFound;
    case OpenExtensionOutcome::kFailureOpenInNotFound:
      return kOpenExtensionOutcomeFailureOpenInNotFound;
    case OpenExtensionOutcome::kFailureUnsupportedScheme:
      return kOpenExtensionOutcomeFailureUnsupportedScheme;
    case OpenExtensionOutcome::kInvalid:
      NOTREACHED();
  }
}

OpenExtensionOutcome OutcomeTypeFromKey(NSString* key) {
  if ([key isEqualToString:kOpenExtensionOutcomeSuccess]) {
    return OpenExtensionOutcome::kSuccess;
  }
  if ([key isEqualToString:kOpenExtensionOutcomeFailureInvalidURL]) {
    return OpenExtensionOutcome::kFailureInvalidURL;
  }
  if ([key isEqualToString:kOpenExtensionOutcomeFailureURLNotFound]) {
    return OpenExtensionOutcome::kFailureURLNotFound;
  }
  if ([key isEqualToString:kOpenExtensionOutcomeFailureOpenInNotFound]) {
    return OpenExtensionOutcome::kFailureOpenInNotFound;
  }
  if ([key isEqualToString:kOpenExtensionOutcomeFailureUnsupportedScheme]) {
    return OpenExtensionOutcome::kFailureUnsupportedScheme;
  }
  return OpenExtensionOutcome::kInvalid;
}

}  // namespace app_group
