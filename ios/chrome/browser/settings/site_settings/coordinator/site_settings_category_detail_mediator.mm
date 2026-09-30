// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/site_settings/coordinator/site_settings_category_detail_mediator.h"

#import <string_view>

#import "base/auto_reset.h"
#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/browser/content_settings_uma_util.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/content_settings/model/content_settings_observer_bridge.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/site_settings/public/site_settings_constants.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_category_detail_consumer.h"
#import "ios/chrome/browser/settings/site_settings/ui/site_settings_site_exception.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

namespace {

// Histogram recorded when a default content setting is changed.
constexpr char kSiteSettingsChangedHistogram[] =
    "Permissions.SiteSettingsChanged";

// Histograms recorded when an action is taken on a site exception.
constexpr std::string_view kExceptionActionMicrophoneHistogram =
    "IOS.SiteSettings.ExceptionAction.Microphone";
constexpr std::string_view kExceptionActionCameraHistogram =
    "IOS.SiteSettings.ExceptionAction.Camera";
constexpr std::string_view kExceptionActionLocationHistogram =
    "IOS.SiteSettings.ExceptionAction.Location";

// Records `action` for the given `type` in the category's exception action
// histogram.
void RecordExceptionAction(SiteSettingsExceptionAction action,
                           ContentSettingsType type) {
  std::string_view histogram_name;
  switch (type) {
    case ContentSettingsType::MEDIASTREAM_MIC:
      histogram_name = kExceptionActionMicrophoneHistogram;
      break;
    case ContentSettingsType::MEDIASTREAM_CAMERA:
      histogram_name = kExceptionActionCameraHistogram;
      break;
    case ContentSettingsType::GEOLOCATION:
      histogram_name = kExceptionActionLocationHistogram;
      break;
    default:
      NOTREACHED();
  }
  base::UmaHistogramEnumeration(histogram_name, action);
}

}  // namespace

@interface SiteSettingsCategoryDetailMediator () <ContentSettingsObserving>
@end

@implementation SiteSettingsCategoryDetailMediator {
  raw_ptr<HostContentSettingsMap> _settingsMap;
  raw_ptr<FaviconLoader> _faviconLoader;
  ContentSettingsType _type;
  std::unique_ptr<ContentSettingsObserverBridge> _settingsObserver;
  BOOL _ignoringSettingsChanges;
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
  if (_ignoringSettingsChanges) {
    return;
  }
  if (contentTypeSet.Contains(_type)) {
    [self loadSettings];
  }
}

#pragma mark - SiteSettingsCategoryDetailMutator

- (void)setDefaultSetting:(ContentSetting)setting {
  if (!_settingsMap) {
    return;
  }
  ContentSetting currentSetting =
      _settingsMap->GetDefaultContentSetting(_type, /*provider_id=*/nullptr);
  if (currentSetting == setting) {
    return;
  }
  _settingsMap->SetDefaultContentSetting(_type, setting);
  content_settings_uma_util::RecordContentSettingsHistogram(
      kSiteSettingsChangedHistogram, _type);
  content_settings_uma_util::RecordContentSettingChange(setting, _type);
}

- (void)setSetting:(ContentSetting)setting
           forSite:(SiteSettingsSiteException*)site {
  if (!_settingsMap || site.setting == setting) {
    return;
  }
  _settingsMap->SetContentSettingCustomScope(
      site.primaryPattern, site.secondaryPattern, _type, setting);
  SiteSettingsExceptionAction action =
      setting == CONTENT_SETTING_ALLOW ? SiteSettingsExceptionAction::kAllowed
                                       : SiteSettingsExceptionAction::kBlocked;
  RecordExceptionAction(action, _type);
}

- (void)deleteSettingForSite:(SiteSettingsSiteException*)site {
  if (!_settingsMap) {
    return;
  }
  _settingsMap->SetContentSettingCustomScope(site.primaryPattern,
                                             site.secondaryPattern, _type,
                                             CONTENT_SETTING_DEFAULT);
  RecordExceptionAction(SiteSettingsExceptionAction::kDeleted, _type);
}

- (void)deleteSettingsForSites:(NSArray<SiteSettingsSiteException*>*)sites {
  if (!_settingsMap || sites.count == 0) {
    return;
  }
  {
    base::AutoReset<BOOL> ignoreChanges(&_ignoringSettingsChanges, YES);
    for (SiteSettingsSiteException* site in sites) {
      _settingsMap->SetContentSettingCustomScope(site.primaryPattern,
                                                 site.secondaryPattern, _type,
                                                 CONTENT_SETTING_DEFAULT);
      RecordExceptionAction(SiteSettingsExceptionAction::kDeleted, _type);
    }
  }
  [self loadSettings];
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
    exception.setting = setting;
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
