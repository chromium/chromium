// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_mediator.h"

#import <set>
#import <string>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/content_settings/core/browser/content_settings_observer.h"
#import "components/content_settings/core/browser/content_settings_registry.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/content_settings/core/common/content_settings.h"
#import "components/content_settings/core/common/content_settings_pattern.h"
#import "components/content_settings/core/common/content_settings_types.h"
#import "components/url_formatter/elide_url.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/site_permissions/site_permissions_site_item.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace {

// C++ observer bridge to listen for ContentSettings changes.
class SitePermissionsContentSettingsObserverBridge
    : public content_settings::Observer {
 public:
  explicit SitePermissionsContentSettingsObserverBridge(
      SitePermissionsMediator* mediator)
      : mediator_(mediator) {}

  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override {
    if (content_type_set.Contains(ContentSettingsType::MEDIASTREAM_MIC) ||
        content_type_set.Contains(ContentSettingsType::MEDIASTREAM_CAMERA) ||
        content_type_set.Contains(ContentSettingsType::GEOLOCATION)) {
      [mediator_ loadSitePermissions];
    }
  }

 private:
  __weak SitePermissionsMediator* mediator_ = nil;
};

}  // namespace

@implementation SitePermissionsMediator {
  raw_ptr<HostContentSettingsMap> _settingsMap;
  raw_ptr<FaviconLoader> _faviconLoader;
  std::unique_ptr<SitePermissionsContentSettingsObserverBridge>
      _settingsObserver;
}

- (instancetype)initWithHostContentSettingsMap:
                    (HostContentSettingsMap*)settingsMap
                                 faviconLoader:(FaviconLoader*)faviconLoader {
  self = [super init];
  if (self) {
    CHECK(settingsMap);
    _settingsMap = settingsMap;
    _faviconLoader = faviconLoader;
    _settingsObserver =
        std::make_unique<SitePermissionsContentSettingsObserverBridge>(self);
    _settingsMap->AddObserver(_settingsObserver.get());
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

#pragma mark - Public

- (void)setConsumer:(id<SitePermissionsConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self loadSitePermissions];
  }
}

- (void)disconnect {
  if (_settingsMap && _settingsObserver) {
    _settingsMap->RemoveObserver(_settingsObserver.get());
  }
  _settingsObserver.reset();
  _settingsMap = nullptr;
  _faviconLoader = nullptr;
  _consumer = nil;
}

- (void)loadSitePermissions {
  if (!_settingsMap) {
    return;
  }

  std::set<std::string> uniqueOrigins;
  const ContentSettingsType trackedTypes[] = {
      ContentSettingsType::MEDIASTREAM_MIC,
      ContentSettingsType::MEDIASTREAM_CAMERA,
      ContentSettingsType::GEOLOCATION,
  };

  for (ContentSettingsType type : trackedTypes) {
    if (!content_settings::ContentSettingsRegistry::GetInstance()->Get(type)) {
      continue;
    }
    ContentSettingsForOneType settings =
        _settingsMap->GetSettingsForOneType(type);
    for (const auto& entry : settings) {
      if (entry.primary_pattern == ContentSettingsPattern::Wildcard()) {
        continue;
      }
      if (entry.IsExpired()) {
        continue;
      }
      GURL repUrl = entry.primary_pattern.ToRepresentativeUrl();
      if (repUrl.is_valid()) {
        uniqueOrigins.insert(url::Origin::Create(repUrl).Serialize());
      } else {
        uniqueOrigins.insert(entry.primary_pattern.ToString());
      }
    }
  }

  NSMutableArray<SitePermissionsSiteItem*>* items = [NSMutableArray array];
  for (const std::string& originStr : uniqueOrigins) {
    SitePermissionsSiteItem* item = [[SitePermissionsSiteItem alloc] init];
    item.origin = base::SysUTF8ToNSString(originStr);
    GURL url(originStr);
    if (url.is_valid()) {
      item.formattedTitle = base::SysUTF16ToNSString(
          url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
              url));
      item.URL = [[CrURL alloc] initWithGURL:url];
    } else {
      item.formattedTitle = item.origin;
    }
    [items addObject:item];
  }

  [items sortUsingComparator:^NSComparisonResult(SitePermissionsSiteItem* a,
                                                 SitePermissionsSiteItem* b) {
    return [a.formattedTitle localizedCaseInsensitiveCompare:b.formattedTitle];
  }];

  [self.consumer setSitePermissionsSiteItems:items];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes* attributes,
                                    bool cached))completion {
  if (!_faviconLoader) {
    return;
  }
  _faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, completion);
}

@end
