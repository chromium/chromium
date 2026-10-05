// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios.h"

#import "base/time/time.h"
#import "components/lens/lens_overlay_invocation_source.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#import "ui/base/l10n/l10n_util.h"

AIModeButtonServiceIOS::AIModeButtonServiceIOS(
    TemplateURLService* template_url_service)
    : template_url_service_(template_url_service) {}

AIModeButtonServiceIOS::~AIModeButtonServiceIOS() = default;

NSString* AIModeButtonServiceIOS::GetTitle() const {
  return l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM);
}

UIImage* AIModeButtonServiceIOS::GetIcon() const {
  if (IsNewTabPageUICleanupEnabled()) {
    UIImageSymbolConfiguration* symbol_configuration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:kQuickActionsSymbolPointSizeUICleanup
                                weight:UIImageSymbolWeightSemibold];
    return MakeSymbolMonochrome(SymbolWithConfiguration(
        SymbolMagnifyingglassSpark, symbol_configuration));
  }
  return MakeSymbolMonochrome(SymbolWithPointSize(
      SymbolMagnifyingglassSpark, kQuickActionsSymbolPointSize));
}

GURL AIModeButtonServiceIOS::GetUrl() const {
  if (!template_url_service_) {
    return GURL();
  }
  return GetUrlForAim(template_url_service_,
                      omnibox::IOS_CHROME_NTP_FAKE_OMNIBOX_ENTRY_POINT,
                      base::Time::Now(),
                      /*query_text=*/u"",
                      lens::LensOverlayInvocationSource::kNtpContextualQuery,
                      /*additional_params=*/{});
}
