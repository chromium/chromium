// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/ai_mode_button_service_ios.h"

#import "base/memory/raw_ptr.h"
#import "base/time/time.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation AIModeButtonServiceIOS {
  raw_ptr<TemplateURLService> _templateURLService;
}

- (instancetype)initWithTemplateURLService:
    (TemplateURLService*)templateURLService {
  self = [super init];
  if (self) {
    _templateURLService = templateURLService;
  }
  return self;
}

#pragma mark - Properties

- (NSString*)title {
  return l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM);
}

- (UIImage*)icon {
  if (IsNewTabPageUICleanupEnabled()) {
    UIImageSymbolConfiguration* symbolConfiguration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:kQuickActionsSymbolPointSizeUICleanup
                                weight:UIImageSymbolWeightSemibold];
    return MakeSymbolMonochrome(SymbolWithConfiguration(
        SymbolMagnifyingglassSpark, symbolConfiguration));
  }
  return MakeSymbolMonochrome(SymbolWithPointSize(
      SymbolMagnifyingglassSpark, kQuickActionsSymbolPointSize));
}

- (GURL)URL {
  if (!_templateURLService) {
    return GURL();
  }
  return GetUrlForAim(_templateURLService, base::Time::Now());
}

@end
