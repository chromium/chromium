// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ai_mode_button_service_ios.h"

#import <string_view>

#import "base/auto_reset.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/favicon/core/favicon_service.h"
#import "components/favicon_base/favicon_types.h"
#import "components/image_fetcher/core/image_fetcher.h"
#import "components/image_fetcher/core/image_fetcher_service.h"
#import "components/image_fetcher/core/request_metadata.h"
#import "components/lens/lens_overlay_invocation_source.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/search/search.h"
#import "components/search_engines/ai_mode_button_service.h"
#import "components/search_engines/search_engine_type.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/image/image.h"

namespace {

constexpr std::string_view kEntrypointShownHistogram =
    "Omnibox.AimEntrypoint.Shown";
constexpr std::string_view kEntrypointShownGoogleHistogram =
    "Omnibox.AimEntrypoint.Shown.google";
constexpr std::string_view kEntrypointShown3pHistogram =
    "Omnibox.AimEntrypoint.Shown.3p";
constexpr std::string_view kIconSourceHistogram =
    "Omnibox.AiModePageAction.IconSource";

void RecordIconSource(AIModeButtonServiceIOS::IconSource source) {
  base::UmaHistogramEnumeration(kIconSourceHistogram, source);
}

// Returns the point size for the AIM quick action button icon.
CGFloat GetAimButtonIconPointSize() {
  return IsNewTabPageUICleanupEnabled() ? kQuickActionsSymbolPointSizeUICleanup
                                        : kQuickActionsSymbolPointSize;
}

}  // namespace

AIModeButtonServiceIOS::AIModeButtonServiceIOS(
    TemplateURLService* template_url_service,
    AimEligibilityService* aim_eligibility_service,
    AiModeButtonService* ai_mode_button_service,
    FaviconLoader* favicon_loader,
    favicon::FaviconService* favicon_service,
    image_fetcher::ImageFetcherService* image_fetcher_service)
    : template_url_service_(template_url_service),
      aim_eligibility_service_(aim_eligibility_service),
      ai_mode_button_service_(ai_mode_button_service),
      favicon_loader_(favicon_loader),
      favicon_service_(favicon_service),
      image_fetcher_service_(image_fetcher_service) {
  if (template_url_service_) {
    template_url_service_observation_.Observe(template_url_service_);
  }
  if (aim_eligibility_service_) {
    eligibility_subscription_ =
        aim_eligibility_service_->RegisterEligibilityChangedCallback(
            base::BindRepeating(&AIModeButtonServiceIOS::OnEligibilityChanged,
                                base::Unretained(this)));
  }
  if (ai_mode_button_service_) {
    ai_mode_button_subscription_ =
        ai_mode_button_service_->RegisterOnConfigChanged(base::BindRepeating(
            &AIModeButtonServiceIOS::OnAiModeButtonConfigChanged,
            base::Unretained(this)));
  }
}

AIModeButtonServiceIOS::~AIModeButtonServiceIOS() = default;

void AIModeButtonServiceIOS::Shutdown() {
  favicon_fetch_weak_factory_.InvalidateWeakPtrs();
  template_url_service_observation_.Reset();
  eligibility_subscription_ = {};
  ai_mode_button_subscription_ = {};
  template_url_service_ = nullptr;
  aim_eligibility_service_ = nullptr;
  ai_mode_button_service_ = nullptr;
  favicon_loader_ = nullptr;
  favicon_service_ = nullptr;
  image_fetcher_service_ = nullptr;
  current_favicon_ = nil;
}

bool AIModeButtonServiceIOS::IsButtonAvailable() const {
  const bool allowed_on_device =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE ||
      IsAIMNTPEntrypointTabletEnabled();
  if (!allowed_on_device) {
    return false;
  }
  return OmniboxFieldTrial::IsAimOmniboxEntrypointEnabled(
      aim_eligibility_service_, ai_mode_button_service_, template_url_service_);
}

void AIModeButtonServiceIOS::RecordEntrypointShown(bool shown) const {
  base::UmaHistogramBoolean(kEntrypointShownHistogram, shown);
  if (!ai_mode_button_service_) {
    return;
  }
  const AiModeButtonUiConfig* config =
      ai_mode_button_service_->GetCurrentConfig();
  if (!config) {
    return;
  }
  base::UmaHistogramBoolean(config->id == SearchEngineType::SEARCH_ENGINE_GOOGLE
                                ? kEntrypointShownGoogleHistogram
                                : kEntrypointShown3pHistogram,
                            shown);
}

base::CallbackListSubscription
AIModeButtonServiceIOS::RegisterStateChangedCallback(
    base::RepeatingClosure callback) {
  return state_changed_callbacks_.Add(std::move(callback));
}

void AIModeButtonServiceIOS::OnTemplateURLServiceChanged() {
  UpdateFavicon();
  NotifyStateChanged();
}

void AIModeButtonServiceIOS::OnTemplateURLServiceShuttingDown() {
  template_url_service_observation_.Reset();
  template_url_service_ = nullptr;
}

void AIModeButtonServiceIOS::OnEligibilityChanged() {
  UpdateFavicon();
  NotifyStateChanged();
}

void AIModeButtonServiceIOS::OnAiModeButtonConfigChanged(
    const AiModeButtonUiConfig* /*config*/) {
  UpdateFavicon();
  NotifyStateChanged();
}

void AIModeButtonServiceIOS::UpdateFavicon() {
  base::AutoReset<bool> auto_reset(&updating_favicon_, true);
  const AiModeButtonUiConfig* config = GetThirdPartyConfig();
  if (config && !IsButtonAvailable()) {
    config = nullptr;
  }
  GURL favicon_url = config ? GURL(config->favicon_url) : GURL();
  if (!favicon_url.is_valid()) {
    if (config) {
      RecordIconSource(IconSource::kFailedIcon);
    }
    favicon_url = GURL();
  }
  if (favicon_url == current_favicon_url_) {
    return;
  }

  favicon_fetch_weak_factory_.InvalidateWeakPtrs();
  current_favicon_ = nil;
  current_favicon_url_ = favicon_url;
  newly_loaded_favicon_ = false;
  if (!favicon_url.is_valid()) {
    return;
  }
  if (!favicon_loader_) {
    RecordIconSource(IconSource::kFailedIcon);
    return;
  }

  const CGFloat icon_size = GetAimButtonIconPointSize();
  favicon_loader_->FaviconForIconUrl(
      favicon_url, icon_size, icon_size,
      base::CallbackToBlock(base::BindRepeating(
          &AIModeButtonServiceIOS::OnFaviconLoaded,
          favicon_fetch_weak_factory_.GetWeakPtr(), favicon_url)));
}

void AIModeButtonServiceIOS::OnFaviconLoaded(const GURL& favicon_url,
                                             FaviconAttributes* attributes,
                                             bool cached) {
  if (attributes.faviconImage) {
    RecordIconSource(cached ? IconSource::kMemoryFaviconCache
                            : IconSource::kDiskDbFaviconCache);
    current_favicon_ = [attributes.faviconImage
        imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
    newly_loaded_favicon_ = true;
    if (!updating_favicon_) {
      NotifyStateChanged();
    }
    return;
  }
  // Ignore the synchronous monogram placeholder returned before the async
  // FaviconService database lookup completes.
  if (cached) {
    return;
  }
  FetchFaviconFromNetwork(favicon_url);
}

void AIModeButtonServiceIOS::FetchFaviconFromNetwork(const GURL& favicon_url) {
  image_fetcher::ImageFetcher* fetcher =
      image_fetcher_service_
          ? image_fetcher_service_->GetImageFetcher(
                image_fetcher::ImageFetcherConfig::kNetworkOnly)
          : nullptr;
  if (!fetcher) {
    RecordIconSource(IconSource::kFailedIcon);
    return;
  }
  fetcher->FetchImage(
      favicon_url,
      base::BindOnce(&AIModeButtonServiceIOS::OnFaviconFetchedFromNetwork,
                     favicon_fetch_weak_factory_.GetWeakPtr(), favicon_url),
      image_fetcher::ImageFetcherParams(NO_TRAFFIC_ANNOTATION_YET, "Omnibox"));
}

void AIModeButtonServiceIOS::OnFaviconFetchedFromNetwork(
    const GURL& favicon_url,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& /*metadata*/) {
  if (image.IsEmpty()) {
    RecordIconSource(IconSource::kFailedIcon);
    return;
  }
  if (favicon_service_) {
    favicon_service_->SetFavicons({favicon_url}, favicon_url,
                                  favicon_base::IconType::kFavicon, image);
  }
  UIImage* ui_image = image.ToUIImage();
  const CGFloat icon_size = GetAimButtonIconPointSize();
  const CGSize target_size = CGSizeMake(icon_size, icon_size);
  if (!CGSizeEqualToSize(ui_image.size, target_size)) {
    ui_image = ResizeImage(ui_image, target_size, ProjectionMode::kAspectFit);
  }
  RecordIconSource(IconSource::kNetworkFetch);
  current_favicon_ =
      [ui_image imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal];
  newly_loaded_favicon_ = true;
  if (!updating_favicon_) {
    NotifyStateChanged();
  }
}

void AIModeButtonServiceIOS::NotifyStateChanged() {
  state_changed_callbacks_.Notify();
  newly_loaded_favicon_ = false;
}

const AiModeButtonUiConfig* AIModeButtonServiceIOS::GetThirdPartyConfig()
    const {
  if (!template_url_service_ || !ai_mode_button_service_ ||
      search::DefaultSearchProviderIsGoogle(template_url_service_)) {
    return nullptr;
  }
  return ai_mode_button_service_->GetCurrentConfig();
}

NSString* AIModeButtonServiceIOS::GetTitle() const {
  if (const AiModeButtonUiConfig* config = GetThirdPartyConfig()) {
    if (!config->text.empty()) {
      return base::SysUTF16ToNSString(config->text);
    }
  }
  return l10n_util::GetNSString(IDS_IOS_NTP_QUICK_ACTIONS_AIM);
}

NSString* AIModeButtonServiceIOS::GetAccessibilityLabel() const {
  if (const AiModeButtonUiConfig* config = GetThirdPartyConfig()) {
    if (!config->a11y_label.empty()) {
      return base::SysUTF16ToNSString(config->a11y_label);
    }
  }
  return GetTitle();
}

UIImage* AIModeButtonServiceIOS::GetIcon() const {
  const bool is_google =
      !template_url_service_ ||
      search::DefaultSearchProviderIsGoogle(template_url_service_);
  if (!IsButtonAvailable()) {
    RecordIconSource(IconSource::kInvisible);
  } else if (is_google) {
    RecordIconSource(IconSource::kVectorIcon);
  } else if (current_favicon_) {
    if (!newly_loaded_favicon_) {
      RecordIconSource(IconSource::kMemoryFaviconCache);
    }
    return current_favicon_;
  }
  Symbol symbol = is_google ? SymbolMagnifyingglassSpark : SymbolSearch;
  if (IsNewTabPageUICleanupEnabled()) {
    UIImageSymbolConfiguration* symbol_configuration =
        [UIImageSymbolConfiguration
            configurationWithPointSize:kQuickActionsSymbolPointSizeUICleanup
                                weight:UIImageSymbolWeightSemibold];
    return MakeSymbolMonochrome(
        SymbolWithConfiguration(symbol, symbol_configuration));
  }
  return MakeSymbolMonochrome(
      SymbolWithPointSize(symbol, kQuickActionsSymbolPointSize));
}

GURL AIModeButtonServiceIOS::GetUrl() const {
  if (const AiModeButtonUiConfig* config = GetThirdPartyConfig()) {
    return GURL(config->navigation_url_empty);
  }
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
