// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

#import <Foundation/Foundation.h>

#import "base/base64.h"
#import "base/logging.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "third_party/skia/include/core/SkColor.h"
#import "url/gurl.h"

namespace {

// Maximum number of recently used backgrounds to store.
const int kMaxRecentlyUsedBackgrounds = 7;

}  // namespace

bool operator==(RecentlyUsedBackgroundInternal const& lhs,
                RecentlyUsedBackgroundInternal const& rhs) {
  if (std::holds_alternative<sync_pb::ThemeSpecificsIos>(lhs) &&
      std::holds_alternative<sync_pb::ThemeSpecificsIos>(rhs)) {
    return std::get<sync_pb::ThemeSpecificsIos>(lhs) ==
           std::get<sync_pb::ThemeSpecificsIos>(rhs);
  } else if (std::holds_alternative<HomeUserUploadedBackground>(lhs) &&
             std::holds_alternative<HomeUserUploadedBackground>(rhs)) {
    return std::get<HomeUserUploadedBackground>(lhs) ==
           std::get<HomeUserUploadedBackground>(rhs);
  }
  return false;
}

bool operator==(sync_pb::NtpCustomBackground const& lhs,
                sync_pb::NtpCustomBackground const& rhs) {
  return lhs.url() == rhs.url();
}

bool operator==(sync_pb::UserColorTheme const& lhs,
                sync_pb::UserColorTheme const& rhs) {
  return lhs.color() == rhs.color() &&
         lhs.browser_color_variant() == rhs.browser_color_variant();
}

bool operator==(sync_pb::ThemeSpecificsIos const& lhs,
                sync_pb::ThemeSpecificsIos const& rhs) {
  // Ntp Background field takes precedence. Only compare colors if neither
  // theme has an ntp background.
  if (lhs.has_ntp_background() != rhs.has_ntp_background()) {
    return false;
  }

  // Only compare url.
  if (lhs.has_ntp_background()) {
    return lhs.ntp_background() == rhs.ntp_background();
  }

  if (lhs.has_user_color_theme() != rhs.has_user_color_theme()) {
    return false;
  }

  return lhs.user_color_theme() == rhs.user_color_theme();
}

HomeBackgroundCustomizationService::HomeBackgroundCustomizationService(
    PrefService* pref_service)
    : recently_used_backgrounds_(
          base::HashingLRUCacheSet<
              RecentlyUsedBackgroundInternal>::NO_AUTO_EVICT),
      pref_service_(pref_service) {
  LoadCurrentTheme();

  const base::Value::List& recently_used_backgrounds_list =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);
  for (const base::Value& background_value : recently_used_backgrounds_list) {
    if (background_value.is_string()) {
      recently_used_backgrounds_.Put(
          DecodeThemeSpecificsIos(background_value.GetString()));
    } else if (background_value.is_dict()) {
      std::optional<HomeUserUploadedBackground> user_background =
          HomeUserUploadedBackground::FromDict(background_value.GetDict());
      if (user_background) {
        recently_used_backgrounds_.Put(user_background.value());
      }
    }
  }

  std::optional<RecentlyUsedBackgroundInternal> current_background =
      std::nullopt;
  if (GetCurrentNtpCustomBackground() || GetCurrentColorTheme()) {
    current_background = current_theme_;
  }
  if (GetCurrentUserUploadedBackground()) {
    current_background = GetCurrentUserUploadedBackground().value();
  }
  if (current_background) {
    // Make sure the current background is first in the recently used list.
    AddToRecentlyUsedBackgroundsList(std::move(current_background.value()));
  }
}

HomeBackgroundCustomizationService::~HomeBackgroundCustomizationService() {}

void HomeBackgroundCustomizationService::Shutdown() {}

void HomeBackgroundCustomizationService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kIosSavedThemeSpecificsIos,
                               std::string());
  registry->RegisterDictionaryPref(prefs::kIosUserUploadedBackground);
  registry->RegisterListPref(prefs::kIosRecentlyUsedBackgrounds);
}

std::optional<HomeCustomBackground>
HomeBackgroundCustomizationService::GetCurrentCustomBackground() {
  std::optional<HomeUserUploadedBackground> user_uploaded_background =
      GetCurrentUserUploadedBackground();
  if (user_uploaded_background) {
    return user_uploaded_background;
  }
  return GetCurrentNtpCustomBackground();
}

std::optional<sync_pb::NtpCustomBackground>
HomeBackgroundCustomizationService::GetCurrentNtpCustomBackground() {
  if (!current_theme_.has_ntp_background()) {
    return std::nullopt;
  }
  return current_theme_.ntp_background();
}

std::optional<sync_pb::UserColorTheme>
HomeBackgroundCustomizationService::GetCurrentColorTheme() {
  if (!current_theme_.has_user_color_theme()) {
    return std::nullopt;
  }
  return current_theme_.user_color_theme();
}

std::vector<RecentlyUsedBackground>
HomeBackgroundCustomizationService::GetRecentlyUsedBackgrounds() {
  std::vector<RecentlyUsedBackground> backgrounds;
  for (const RecentlyUsedBackgroundInternal& background :
       recently_used_backgrounds_) {
    backgrounds.push_back(ConvertBackgroundRepresentation(background));
  }
  return backgrounds;
}

void HomeBackgroundCustomizationService::SetCurrentBackground(
    const GURL& background_url,
    const GURL& thumbnail_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& attribution_action_url,
    const std::string& collection_id) {
  sync_pb::NtpCustomBackground new_background;
  new_background.set_url(background_url.spec());
  new_background.set_attribution_line_1(attribution_line_1);
  new_background.set_attribution_line_2(attribution_line_2);
  new_background.set_attribution_action_url(attribution_action_url.spec());
  new_background.set_collection_id(collection_id);

  *current_theme_.mutable_ntp_background() = new_background;
  current_theme_.clear_user_color_theme();

  ClearCurrentUserUploadedBackground();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::SetBackgroundColor(
    SkColor color,
    sync_pb::UserColorTheme::BrowserColorVariant color_variant) {
  sync_pb::UserColorTheme new_color_theme;
  new_color_theme.set_color(color);
  new_color_theme.set_browser_color_variant(color_variant);

  *current_theme_.mutable_user_color_theme() = new_color_theme;
  current_theme_.clear_ntp_background();

  ClearCurrentUserUploadedBackground();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::ClearCurrentBackground() {
  current_theme_.Clear();

  ClearCurrentUserUploadedBackground();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::StoreCurrentTheme() {
  // Only update recently used backgrounds list if the background is not
  // default.
  std::optional<RecentlyUsedBackgroundInternal> new_recent_background =
      std::nullopt;
  if (GetCurrentNtpCustomBackground() || GetCurrentColorTheme()) {
    new_recent_background = current_theme_;
  }

  pref_service_->SetString(prefs::kIosSavedThemeSpecificsIos,
                           EncodeThemeSpecificsIos(current_theme_));

  if (current_user_uploaded_background_) {
    pref_service_->SetDict(prefs::kIosUserUploadedBackground,
                           current_user_uploaded_background_->ToDict());
    new_recent_background = current_user_uploaded_background_.value();
  } else {
    pref_service_->ClearPref(prefs::kIosUserUploadedBackground);
  }

  if (new_recent_background) {
    AddToRecentlyUsedBackgroundsList(std::move(new_recent_background.value()));
  }

  base::Value::List recently_used_backgrounds_list;
  for (const RecentlyUsedBackgroundInternal& background :
       recently_used_backgrounds_) {
    if (std::holds_alternative<sync_pb::ThemeSpecificsIos>(background)) {
      sync_pb::ThemeSpecificsIos theme =
          std::get<sync_pb::ThemeSpecificsIos>(background);
      recently_used_backgrounds_list.Append(EncodeThemeSpecificsIos(theme));
    } else {
      HomeUserUploadedBackground userBackground =
          std::get<HomeUserUploadedBackground>(background);
      recently_used_backgrounds_list.Append(userBackground.ToDict());
    }
  }
  pref_service_->SetList(prefs::kIosRecentlyUsedBackgrounds,
                         std::move(recently_used_backgrounds_list));
}

void HomeBackgroundCustomizationService::RestoreCurrentTheme() {
  LoadCurrentTheme();

  std::optional<sync_pb::UserColorTheme> colorTheme = GetCurrentColorTheme();
  std::optional<sync_pb::NtpCustomBackground> presetImage =
      GetCurrentNtpCustomBackground();
  std::optional<HomeUserUploadedBackground> uploadedImage =
      GetCurrentUserUploadedBackground();

  if (colorTheme) {
    SetBackgroundColor(colorTheme->color(),
                       colorTheme->browser_color_variant());
  } else if (presetImage) {
    SetCurrentBackground(GURL(presetImage->url()), GURL(presetImage->url()),
                         presetImage->attribution_line_1(),
                         presetImage->attribution_line_2(),
                         GURL(presetImage->attribution_action_url()),
                         presetImage->collection_id());
  } else if (uploadedImage) {
    SetCurrentUserUploadedBackground(uploadedImage->image_path,
                                     uploadedImage->framing_coordinates);
  } else {
    ClearCurrentBackground();
  }
}

void HomeBackgroundCustomizationService::LoadCurrentTheme() {
  current_theme_ = DecodeThemeSpecificsIos(
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos));

  const base::Value::Dict& background_data =
      pref_service_->GetDict(prefs::kIosUserUploadedBackground);

  current_user_uploaded_background_ =
      HomeUserUploadedBackground::FromDict(background_data);
}

void HomeBackgroundCustomizationService::AddObserver(
    HomeBackgroundCustomizationServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void HomeBackgroundCustomizationService::RemoveObserver(
    HomeBackgroundCustomizationServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HomeBackgroundCustomizationService::NotifyObserversOfBackgroundChange() {
  for (HomeBackgroundCustomizationServiceObserver& observer : observers_) {
    observer.OnBackgroundChanged();
  }
}

std::optional<HomeUserUploadedBackground>
HomeBackgroundCustomizationService::GetCurrentUserUploadedBackground() {
  return current_user_uploaded_background_;
}

void HomeBackgroundCustomizationService::SetCurrentUserUploadedBackground(
    const std::string& image_path,
    const FramingCoordinates& framing_coordinates) {
  HomeUserUploadedBackground background;
  background.image_path = image_path;
  background.framing_coordinates = framing_coordinates;
  current_user_uploaded_background_ = background;

  current_theme_.clear_ntp_background();
  current_theme_.clear_user_color_theme();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::ClearCurrentUserUploadedBackground() {
  current_user_uploaded_background_ = std::nullopt;
}

RecentlyUsedBackground
HomeBackgroundCustomizationService::ConvertBackgroundRepresentation(
    RecentlyUsedBackgroundInternal background) {
  if (std::holds_alternative<sync_pb::ThemeSpecificsIos>(background)) {
    sync_pb::ThemeSpecificsIos theme_specifics =
        std::get<sync_pb::ThemeSpecificsIos>(background);
    if (theme_specifics.has_ntp_background()) {
      return theme_specifics.ntp_background();
    }
    return theme_specifics.user_color_theme();
  } else {
    HomeUserUploadedBackground user_uploaded_background =
        std::get<HomeUserUploadedBackground>(background);
    return user_uploaded_background;
  }
}

std::string HomeBackgroundCustomizationService::EncodeThemeSpecificsIos(
    sync_pb::ThemeSpecificsIos theme_specifics_ios) {
  std::string serialized = theme_specifics_ios.SerializeAsString();
  // Encode bytestring so it can be stored in a pref.
  return base::Base64Encode(serialized);
}

sync_pb::ThemeSpecificsIos
HomeBackgroundCustomizationService::DecodeThemeSpecificsIos(
    std::string encoded) {
  // This pref is base64 encoded, so decode it first.
  std::string serialized;
  base::Base64Decode(encoded, &serialized);
  sync_pb::ThemeSpecificsIos theme_specifics_ios;
  theme_specifics_ios.ParseFromString(serialized);
  return theme_specifics_ios;
}

void HomeBackgroundCustomizationService::AddToRecentlyUsedBackgroundsList(
    RecentlyUsedBackgroundInternal&& recent_background) {
  recently_used_backgrounds_.Put(
      std::forward<RecentlyUsedBackgroundInternal>(recent_background));

  if (recently_used_backgrounds_.size() > kMaxRecentlyUsedBackgrounds) {
    // TODO(crbug.com/438564566): Clean up any backgrounds that leave the list.
    recently_used_backgrounds_.ShrinkToSize(kMaxRecentlyUsedBackgrounds);
  }
}
