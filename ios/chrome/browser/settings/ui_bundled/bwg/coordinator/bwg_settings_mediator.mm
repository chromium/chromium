// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/bwg_settings_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/bwg_settings_consumer.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

@interface BWGSettingsMediator () <BooleanObserver>
@end

@implementation BWGSettingsMediator {
  // Accessor for the location preference.
  PrefBackedBoolean* _preciseLocationPref;
  // Accessor for the page content preference.
  PrefBackedBoolean* _pageContentPref;
  // PrefService.
  raw_ptr<PrefService> _prefService;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    DCHECK(prefService);
    _prefService = prefService;
    _preciseLocationPref = [[PrefBackedBoolean alloc]
        initWithPrefService:prefService
                   prefName:prefs::kIOSBWGPreciseLocationSetting];
    _preciseLocationPref.observer = self;
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

  [_pageContentPref stop];
  _pageContentPref.observer = nil;
  _pageContentPref = nil;
}

- (void)setConsumer:(id<BWGSettingsConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setPreciseLocationEnabled:_preciseLocationPref.value];
  [_consumer setPageContentSharingEnabled:_pageContentPref.value];
}

#pragma mark - BWGSettingsMutator

- (void)openNewTabWithURL:(const GURL&)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.applicationHandler openURLInNewTab:command];
}

- (void)setPreciseLocationPref:(BOOL)value {
  _prefService->SetBoolean(prefs::kIOSBWGPreciseLocationSetting, value);
}

- (void)setPageContentSharingPref:(BOOL)value {
  _prefService->SetBoolean(prefs::kIOSBWGPageContentSetting, value);
  ios::provider::BWGPageContextAttachmentState attachmentState =
      value ? ios::provider::BWGPageContextAttachmentState::kAttached
            : ios::provider::BWGPageContextAttachmentState::kUserDisabled;
  ios::provider::UpdatePageAttachmentState(attachmentState);
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self.consumer setPreciseLocationEnabled:_preciseLocationPref.value];
  [self.consumer setPageContentSharingEnabled:_pageContentPref.value];
}

@end
