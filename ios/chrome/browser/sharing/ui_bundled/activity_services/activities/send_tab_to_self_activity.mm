// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/send_tab_to_self_activity.h"

#import <vector>

#import "base/feature_list.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/send_tab_to_self_entry.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "components/send_tab_to_self/target_device_info.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/send_tab_to_self_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/share_to_data.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Maximum number of target devices to display directly in the Share Sheet.
constexpr size_t kMaxShareSheetTargetDevices = 2;

NSString* const kSendTabToSelfActivityType =
    @"com.google.chrome.sendTabToSelfActivity";

// Returns the SF Symbol name corresponding to the device form factor.
NSString* GetSFSymbolNameForFormFactor(
    syncer::DeviceInfo::FormFactor form_factor) {
  switch (form_factor) {
    case syncer::DeviceInfo::FormFactor::kPhone:
      return kIPhoneSymbol;
    case syncer::DeviceInfo::FormFactor::kTablet:
      return kIPadSymbol;
    default:
      return kLaptopSymbol;
  }
}

// Returns the sorted list of target devices if the Send Tab to Self model is
// ready and the feature flag is enabled.
std::vector<send_tab_to_self::TargetDeviceInfo> GetTargetDevices(
    send_tab_to_self::SendTabToSelfSyncService* sync_service) {
  if (!base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfIOSShareSheetDeviceList)) {
    return {};
  }
  if (!sync_service) {
    return {};
  }
  send_tab_to_self::SendTabToSelfModel* model =
      sync_service->GetSendTabToSelfModel();
  if (!model || !model->IsReady()) {
    return {};
  }
  std::vector<send_tab_to_self::TargetDeviceInfo> devices =
      model->GetTargetDeviceInfoSortedList();
  if (devices.size() > kMaxShareSheetTargetDevices) {
    devices.resize(kMaxShareSheetTargetDevices);
  }
  return devices;
}

}  // namespace

@interface SendTabToSelfActivity ()
// The data object targeted by this activity.
@property(nonatomic, strong, readonly) ShareToData* data;
// The handler to be invoked when the activity is performed.
@property(nonatomic, weak, readonly) id<SendTabToSelfCommands> handler;
@end

@implementation SendTabToSelfActivity

- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<SendTabToSelfCommands>)handler {
  if ((self = [super init])) {
    _data = data;
    _handler = handler;
  }
  return self;
}

#pragma mark - Public Class Methods

+ (NSArray<UIActivity*>*)
    sendTabToSelfActivitiesForData:(ShareToData*)data
                       syncService:(send_tab_to_self::SendTabToSelfSyncService*)
                                       syncService
                           handler:(id<SendTabToSelfCommands>)handler
                     userGivenName:(NSString*)userGivenName {
  if (!send_tab_to_self::SendTabToSelfEntry::IsValidUrl(data.shareURL)) {
    return @[];
  }

  NSMutableArray<UIActivity*>* activities = [[NSMutableArray alloc] init];

  // Always add the generic Send Tab to Self activity as an entry point to the
  // picker UI.
  [activities addObject:[[SendTabToSelfActivity alloc] initWithData:data
                                                            handler:handler]];

  std::vector<send_tab_to_self::TargetDeviceInfo> devices =
      GetTargetDevices(syncService);

  for (const auto& device : devices) {
    NSString* deviceName = base::SysUTF8ToNSString(device.device_name);
    NSString* cacheGuid = base::SysUTF8ToNSString(device.cache_guid);
    NSString* activityTitle = deviceName;
    if (userGivenName.length > 0) {
      activityTitle = l10n_util::GetNSStringF(
          IDS_IOS_SEND_TAB_TO_SELF_SHARE_SHEET_DEVICE_NAME_WITH_OWNER,
          base::SysNSStringToUTF16(userGivenName),
          base::SysNSStringToUTF16(deviceName));
    }
    SendTabToSelfShareActivity* deviceActivity =
        [[SendTabToSelfShareActivity alloc] initWithData:data
                                                 handler:handler
                                           activityTitle:activityTitle
                                               cacheGUID:cacheGuid
                                              deviceName:deviceName
                                              formFactor:device.form_factor];
    [activities addObject:deviceActivity];
  }

  return activities;
}

#pragma mark - UIActivity

- (NSString*)activityType {
  return kSendTabToSelfActivityType;
}

- (NSString*)activityTitle {
  return l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF);
}

- (UIImage*)activityImage {
  return SymbolWithPointSize(SymbolRecentTabs, kSymbolActionPointSize);
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems {
  return self.data.canSendTabToSelf;
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryAction;
}

- (void)performActivity {
  [self activityDidFinish:YES];
  [self.handler
      showSendTabToSelfUI:self.data.shareURL
                    title:self.data.title
               entryPoint:send_tab_to_self::ShareEntryPoint::kShareSheet];
}

@end

#pragma mark - SendTabToSelfShareActivity

@interface SendTabToSelfShareActivity ()
// The custom display title containing the target device name.
@property(nonatomic, strong, readonly) NSString* activityTitleOverride;
// The cache GUID of the specific target device.
@property(nonatomic, strong, readonly) NSString* cacheGUID;
// The display name of the specific target device.
@property(nonatomic, strong, readonly) NSString* deviceName;
// The form factor of the specific target device.
@property(nonatomic, assign, readonly)
    syncer::DeviceInfo::FormFactor formFactor;
@end

@implementation SendTabToSelfShareActivity

- (instancetype)initWithData:(ShareToData*)data
                     handler:(id<SendTabToSelfCommands>)handler
               activityTitle:(NSString*)activityTitle
                   cacheGUID:(NSString*)cacheGUID
                  deviceName:(NSString*)deviceName
                  formFactor:(syncer::DeviceInfo::FormFactor)formFactor {
  if ((self = [super initWithData:data handler:handler])) {
    _activityTitleOverride = activityTitle;
    _cacheGUID = cacheGUID;
    _deviceName = deviceName;
    _formFactor = formFactor;
  }
  return self;
}

#pragma mark - UIActivity Overrides

- (NSString*)activityTitle {
  return self.activityTitleOverride;
}

- (UIImage*)activityImage {
  NSString* symbolName = GetSFSymbolNameForFormFactor(self.formFactor);
  return DefaultSymbolWithPointSize(symbolName, kSymbolActionPointSize);
}

+ (UIActivityCategory)activityCategory {
  return UIActivityCategoryShare;
}

- (void)performActivity {
  [self activityDidFinish:YES];
  [self.handler sendTabToSelfToDeviceWithURL:self.data.shareURL
                                       title:self.data.title
                                    deviceID:self.cacheGUID
                                  deviceName:self.deviceName
                                  entryPoint:send_tab_to_self::ShareEntryPoint::
                                                 kShareSheetDirectShare];
}

@end
