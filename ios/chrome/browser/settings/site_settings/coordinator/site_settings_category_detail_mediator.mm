// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_category_detail_mediator.h"

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/content_settings/model/content_settings_observer_bridge.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_consumer.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_site_exception.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

@interface SiteSettingsCategoryDetailMediator () <ContentSettingsObserving>
@end

@implementation SiteSettingsCategoryDetailMediator {
  raw_ptr<HostContentSettingsMap> _settingsMap;
  raw_ptr<FaviconLoader> _faviconLoader;
  ContentSettingsType _type;
  std::unique_ptr<ContentSettingsObserverBridge> _settingsObserver;
}

- (instancetype)initWithHostContentSettingsMap:
                    (HostContentSettingsMap*)settingsMap
                                 faviconLoader:(FaviconLoader*)faviconLoader
                           contentSettingsType:(ContentSettingsType)type {
  self = [super init];
  if (self) {
    CHECK(settingsMap);
    _settingsMap = settingsMap;
    _faviconLoader = faviconLoader;
    _type = type;
    _settingsObserver =
        std::make_unique<ContentSettingsObserverBridge>(self, _settingsMap);
  }
  return self;
}

#pragma mark - Public

- (void)setConsumer:(id<SiteSettingsCategoryDetailConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self loadSettings];
  }
}

- (void)disconnect {
  _settingsObserver.reset();
  _settingsMap = nullptr;
  _faviconLoader = nullptr;
  _consumer = nil;
}

#pragma mark - ContentSettingsObserving

- (void)contentSettingsMap:(HostContentSettingsMap*)settingsMap
         didChangeForTypes:(ContentSettingsTypeSet)contentTypeSet
            primaryPattern:(const ContentSettingsPattern&)primaryPattern
          secondaryPattern:(const ContentSettingsPattern&)secondaryPattern {
  if (contentTypeSet.Contains(_type)) {
    [self loadSettings];
  }
}

#pragma mark - SiteSettingsCategoryDetailMutator

- (void)setDefaultSetting:(ContentSetting)setting {
  if (!_settingsMap) {
    return;
  }
  _settingsMap->SetDefaultContentSetting(_type, setting);
}

- (void)setSetting:(ContentSetting)setting
           forSite:(SiteSettingsSiteException*)site {
  if (!_settingsMap) {
    return;
  }
  _settingsMap->SetContentSettingCustomScope(
      site.primaryPattern, site.secondaryPattern, _type, setting);
}

- (void)deleteSettingForSite:(SiteSettingsSiteException*)site {
  if (!_settingsMap) {
    return;
  }
  _settingsMap->SetContentSettingCustomScope(site.primaryPattern,
                                             site.secondaryPattern, _type,
                                             CONTENT_SETTING_DEFAULT);
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes* attributes,
                                    bool cached))completion {
  if (!_faviconLoader || !URL || !URL.gurl.is_valid()) {
    return;
  }
  _faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, completion);
}

#pragma mark - Private

// Queries the `HostContentSettingsMap` for default setting and exceptions,
// formats and partitions them into allowed and not allowed lists, and pushes
// them to the consumer.
- (void)loadSettings {
  if (!_settingsMap) {
    return;
  }

  ContentSetting defaultSetting =
      _settingsMap->GetDefaultContentSetting(_type, /*provider_id=*/nullptr);
  [_consumer setDefaultSetting:defaultSetting];

  NSMutableArray<SiteSettingsSiteException*>* allowedExceptions =
      [NSMutableArray array];
  NSMutableArray<SiteSettingsSiteException*>* notAllowedExceptions =
      [NSMutableArray array];

  ContentSettingsForOneType settings =
      _settingsMap->GetSettingsForOneType(_type);
  for (const auto& entry : settings) {
    if (entry.primary_pattern == ContentSettingsPattern::Wildcard()) {
      continue;
    }
    if (entry.IsExpired()) {
      continue;
    }
    SiteSettingsSiteException* exception =
        [[SiteSettingsSiteException alloc] init];
    exception.origin =
        base::SysUTF8ToNSString(entry.primary_pattern.ToString());
    exception.primaryPattern = entry.primary_pattern;
    exception.secondaryPattern = entry.secondary_pattern;
    GURL representativeURL = entry.primary_pattern.ToRepresentativeUrl();
    if (representativeURL.is_valid()) {
      exception.formattedTitle = base::SysUTF16ToNSString(
          url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
              representativeURL));
      exception.URL = [[CrURL alloc] initWithGURL:representativeURL];
    } else {
      exception.formattedTitle = exception.origin;
    }

    ContentSetting setting = entry.GetContentSetting();
    if (setting == CONTENT_SETTING_ALLOW) {
      [allowedExceptions addObject:exception];
    } else if (setting == CONTENT_SETTING_BLOCK) {
      [notAllowedExceptions addObject:exception];
    }
  }

  auto comparator = ^NSComparisonResult(SiteSettingsSiteException* a,
                                        SiteSettingsSiteException* b) {
    return [a.formattedTitle localizedCaseInsensitiveCompare:b.formattedTitle];
  };
  [allowedExceptions sortUsingComparator:comparator];
  [notAllowedExceptions sortUsingComparator:comparator];

  [_consumer setAllowedSites:allowedExceptions
             notAllowedSites:notAllowedExceptions];
}

@end
