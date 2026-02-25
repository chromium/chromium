// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/gemini_settings_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_dynamic_settings_item.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_metadata.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/gemini_settings_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {
// The amount by which to offset the integer value of a dynamic setting to
// create its corresponding section/row identifier.
const NSInteger kDynamicSettingsItemTypeOffset = 10000;
}  // namespace

@interface GeminiSettingsMediator () <BooleanObserver>
@end

@implementation GeminiSettingsMediator {
  // Accessor for the location preference.
  PrefBackedBoolean* _preciseLocationPref;
  // Accessor for the camera permission preference.
  PrefBackedBoolean* _cameraPref;
  // Accessor for the page content preference.
  PrefBackedBoolean* _pageContentPref;
  // AuthenticationService
  raw_ptr<AuthenticationService> _authService;
  // PrefService.
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithAuthService:(AuthenticationService*)authService
                        prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _authService = authService;
    _prefService = prefService;

    _preciseLocationPref = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kIOSBWGPreciseLocationSetting];
    _preciseLocationPref.observer = self;

    _cameraPref = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kIOSGeminiCameraSetting];
    _cameraPref.observer = self;

    _pageContentPref = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kIOSBWGPageContentSetting];
    _pageContentPref.observer = self;
  }
  return self;
}

- (void)disconnect {
  // Stop observable prefs.
  [_preciseLocationPref stop];
  _preciseLocationPref.observer = nil;
  _preciseLocationPref = nil;

  [_cameraPref stop];
  _cameraPref.observer = nil;
  _cameraPref = nil;

  [_pageContentPref stop];
  _pageContentPref.observer = nil;
  _pageContentPref = nil;
}

- (void)setConsumer:(id<GeminiSettingsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setPreciseLocationEnabled:_preciseLocationPref.value];
  [_consumer setCameraPermissionEnabled:_cameraPref.value];
  [_consumer setPageContentSharingEnabled:_pageContentPref.value];
}

- (void)loadDynamicSettings {
  if (IsGeminiDynamicSettingsEnabled()) {
    NSArray<GeminiSettingsMetadata*>* eligibleSettingsMetadata =
        ios::provider::GetEligibleSettings(_authService);

    NSMutableArray<GeminiDynamicSettingsItem*>* settingsItems =
        [[NSMutableArray alloc]
            initWithCapacity:eligibleSettingsMetadata.count];

    for (GeminiSettingsMetadata* setting in eligibleSettingsMetadata) {
      NSInteger type = kDynamicSettingsItemTypeOffset + setting.context;
      GeminiSettingsAction* action =
          ios::provider::ActionForSettingsContext(setting.context);

      GeminiDynamicSettingsItem* item =
          [[GeminiDynamicSettingsItem alloc] initWithType:type
                                                 metadata:setting
                                                   action:action];
      [settingsItems addObject:item];
    }

    [self.consumer updateDynamicSettingsItems:settingsItems];
  }
}

#pragma mark - GeminiSettingsMutator

- (void)openNewTabWithURL:(const GURL&)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.sceneHandler openURLInNewTab:command];
}

- (void)setPreciseLocationPref:(BOOL)value {
  _prefService->SetBoolean(prefs::kIOSBWGPreciseLocationSetting, value);
}

- (void)setCameraPermissionPref:(BOOL)value {
  _prefService->SetBoolean(prefs::kIOSGeminiCameraSetting, value);
}

- (void)setPageContentSharingPref:(BOOL)value {
  _prefService->SetBoolean(prefs::kIOSBWGPageContentSetting, value);
  ios::provider::GeminiPageContextAttachmentState attachmentState =
      value ? ios::provider::GeminiPageContextAttachmentState::kAttached
            : ios::provider::GeminiPageContextAttachmentState::kUserDisabled;
  ios::provider::UpdatePageAttachmentState(attachmentState);
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _preciseLocationPref) {
    [self.consumer setPreciseLocationEnabled:_preciseLocationPref.value];
  } else if (observableBoolean == _cameraPref) {
    [self.consumer setCameraPermissionEnabled:_cameraPref.value];
  } else if (observableBoolean == _pageContentPref) {
    [self.consumer setPageContentSharingEnabled:_pageContentPref.value];
  }
}

@end
