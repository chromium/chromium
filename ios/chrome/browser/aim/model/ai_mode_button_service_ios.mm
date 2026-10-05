// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios.h"

#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/time/time.h"
#import "components/lens/lens_overlay_invocation_source.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

AIModeButtonServiceIOS::AIModeButtonServiceIOS(
    TemplateURLService* template_url_service,
    AimEligibilityService* aim_eligibility_service)
    : template_url_service_(template_url_service),
      aim_eligibility_service_(aim_eligibility_service) {
  if (template_url_service_) {
    template_url_service_observation_.Observe(template_url_service_);
  }
  if (aim_eligibility_service_) {
    eligibility_subscription_ =
        aim_eligibility_service_->RegisterEligibilityChangedCallback(
            base::BindRepeating(&AIModeButtonServiceIOS::OnEligibilityChanged,
                                base::Unretained(this)));
  }
}

AIModeButtonServiceIOS::~AIModeButtonServiceIOS() = default;

void AIModeButtonServiceIOS::Shutdown() {
  template_url_service_observation_.Reset();
  eligibility_subscription_ = {};
  template_url_service_ = nullptr;
  aim_eligibility_service_ = nullptr;
}

bool AIModeButtonServiceIOS::IsButtonAvailable() const {
  const bool allowed_on_device =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE ||
      IsAIMNTPEntrypointTabletEnabled();
  if (!allowed_on_device) {
    return false;
  }
  if (aim_eligibility_service_ && aim_eligibility_service_->IsAimEligible()) {
    return true;
  }
  return false;
}

base::CallbackListSubscription
AIModeButtonServiceIOS::RegisterStateChangedCallback(
    base::RepeatingClosure callback) {
  return state_changed_callbacks_.Add(std::move(callback));
}

void AIModeButtonServiceIOS::OnTemplateURLServiceChanged() {
  NotifyStateChanged();
}

void AIModeButtonServiceIOS::OnTemplateURLServiceShuttingDown() {
  template_url_service_observation_.Reset();
  template_url_service_ = nullptr;
}

void AIModeButtonServiceIOS::OnEligibilityChanged() {
  NotifyStateChanged();
}

void AIModeButtonServiceIOS::NotifyStateChanged() {
  state_changed_callbacks_.Notify();
}

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
