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
  std::string serialized = current_theme_.SerializeAsString();
  // Encode bytestring so it can be stored in a pref.
  std::string encoded = base::Base64Encode(serialized);
  pref_service_->SetString(prefs::kIosSavedThemeSpecificsIos, encoded);

  if (current_user_uploaded_background_) {
    pref_service_->SetDict(prefs::kIosUserUploadedBackground,
                           current_user_uploaded_background_->ToDict());
  } else {
    pref_service_->ClearPref(prefs::kIosUserUploadedBackground);
  }
}

void HomeBackgroundCustomizationService::RestoreCurrentTheme() {
  LoadCurrentTheme();

  std::optional<sync_pb::UserColorTheme> colorTheme = GetCurrentColorTheme();
  std::optional<sync_pb::NtpCustomBackground> presetImage =
      GetCurrentCustomBackground();
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
  }
}

void HomeBackgroundCustomizationService::LoadCurrentTheme() {
  std::string encoded =
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos);
  // This pref is base64 encoded, so decode it first.
  std::string serialized;
  base::Base64Decode(encoded, &serialized);
  current_theme_.ParseFromString(serialized);

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
