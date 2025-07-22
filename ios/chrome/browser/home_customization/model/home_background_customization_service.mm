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
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "third_party/skia/include/core/SkColor.h"
#import "url/gurl.h"

namespace {
// Keys for user-uploaded background dictionary serialization.
const char kImagePathKey[] = "image_path";
const char kFramingDataKey[] = "framing_data";
}  // namespace

HomeBackgroundCustomizationService::HomeBackgroundCustomizationService(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  LoadCurrentTheme();
}

HomeBackgroundCustomizationService::~HomeBackgroundCustomizationService() {}

void HomeBackgroundCustomizationService::Shutdown() {}

void HomeBackgroundCustomizationService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kIosSavedThemeSpecificsIos,
                               std::string());
  registry->RegisterDictionaryPref(prefs::kIosUserUploadedBackground);
}

std::optional<sync_pb::NtpCustomBackground>
HomeBackgroundCustomizationService::GetCurrentCustomBackground() {
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

void HomeBackgroundCustomizationService::SetCurrentBackground(
    const GURL& background_url,
    const GURL& thumbnail_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& attribution_action_url,
    const std::string& collection_id) {
  sync_pb::NtpCustomBackground new_background;
  new_background.set_url(background_url.spec());
  // TODO(crbug.com/411453550): Add thumbnail_url to NtpCustomBackground field.
  new_background.set_attribution_line_1(attribution_line_1);
  new_background.set_attribution_line_2(attribution_line_2);
  new_background.set_attribution_action_url(attribution_action_url.spec());
  new_background.set_collection_id(collection_id);

  *current_theme_.mutable_ntp_background() = new_background;
  current_theme_.clear_user_color_theme();

  pref_service_->ClearPref(prefs::kIosUserUploadedBackground);

  StoreCurrentTheme();
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

  pref_service_->ClearPref(prefs::kIosUserUploadedBackground);

  StoreCurrentTheme();
  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::StoreCurrentTheme() {
  std::string serialized = current_theme_.SerializeAsString();
  // Encode bytestring so it can be stored in a pref.
  std::string encoded = base::Base64Encode(serialized);
  pref_service_->SetString(prefs::kIosSavedThemeSpecificsIos, encoded);
}

void HomeBackgroundCustomizationService::LoadCurrentTheme() {
  std::string encoded =
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos);
  // This pref is base64 encoded, so decode it first.
  std::string serialized;
  base::Base64Decode(encoded, &serialized);
  current_theme_.ParseFromString(serialized);
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

std::optional<std::pair<std::string, FramingCoordinates>>
HomeBackgroundCustomizationService::GetCurrentUserUploadedBackground() {
  const base::Value::Dict& background_data =
      pref_service_->GetDict(prefs::kIosUserUploadedBackground);

  if (background_data.empty()) {
    return std::nullopt;
  }

  const std::string* image_path = background_data.FindString(kImagePathKey);
  const base::Value::Dict* framing_data_dict =
      background_data.FindDict(kFramingDataKey);

  if (!image_path || !framing_data_dict) {
    pref_service_->ClearPref(prefs::kIosUserUploadedBackground);
    return std::nullopt;
  }

  // Convert Dict to FramingCoordinates.
  std::optional<FramingCoordinates> coordinates =
      FramingCoordinates::FromDict(*framing_data_dict);

  if (!coordinates) {
    pref_service_->ClearPref(prefs::kIosUserUploadedBackground);
    return std::nullopt;
  }

  return std::make_pair(*image_path, *coordinates);
}

void HomeBackgroundCustomizationService::SetCurrentUserUploadedBackground(
    const std::string& image_path,
    const FramingCoordinates& framing_coordinates) {
  base::Value::Dict background_data;
  background_data.Set(kImagePathKey, image_path);
  background_data.Set(kFramingDataKey, framing_coordinates.ToDict());

  pref_service_->SetDict(prefs::kIosUserUploadedBackground,
                         std::move(background_data));

  current_theme_.clear_ntp_background();
  current_theme_.clear_user_color_theme();
  StoreCurrentTheme();

  NotifyObserversOfBackgroundChange();
}
