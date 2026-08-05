// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activities/send_tab_to_self_activity.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/send_tab_to_self_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/sharing/ui_bundled/activity_services/data/share_to_data.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

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
