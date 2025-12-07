// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

#import <Foundation/Foundation.h>

#import <set>

#import "base/base64.h"
#import "base/containers/adapters.h"
#import "base/logging.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/sync/protocol/theme_specifics.pb.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "components/themes/pref_names.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "third_party/skia/include/core/SkColor.h"
#import "url/gurl.h"

namespace sync_pb {
bool operator==(const sync_pb::NtpCustomBackground& lhs,
                const sync_pb::NtpCustomBackground& rhs) {
  return lhs.url() == rhs.url();
}

bool operator==(const sync_pb::UserColorTheme& lhs,
                const sync_pb::UserColorTheme& rhs) {
  return lhs.color() == rhs.color() &&
         lhs.browser_color_variant() == rhs.browser_color_variant();
}

bool operator==(const sync_pb::ThemeSpecificsIos& lhs,
                const sync_pb::ThemeSpecificsIos& rhs) {
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
}  // namespace sync_pb

HomeBackgroundCustomizationService::HomeBackgroundCustomizationService(
    PrefService* pref_service,
    UserUploadedImageManager* user_image_manager,
    HomeBackgroundImageService* home_background_image_service)
    : recently_used_backgrounds_(MaxRecentlyUsedBackgrounds()),
      pref_service_(pref_service),
      user_image_manager_(user_image_manager),
      home_background_image_service_(home_background_image_service),
      weak_ptr_factory_{this} {
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
  }
  pref_change_registrar_.Init(pref_service_);
  PrefChangeRegistrar::NamedChangeCallback callback = base::BindRepeating(
      &HomeBackgroundCustomizationService::OnPolicyPrefsChanged,
      weak_ptr_factory_.GetWeakPtr());
  pref_change_registrar_.Add(themes::prefs::kPolicyThemeColor, callback);
  pref_change_registrar_.Add(prefs::kNTPCustomBackgroundEnabledByPolicy,
                             callback);

  LoadCurrentTheme();

  const base::Value::List& recently_used_backgrounds_list =
      pref_service_->GetList(prefs::kIosRecentlyUsedBackgrounds);
  std::set<base::FilePath> image_paths_in_use;

  // The default value for this list is {true}, so use that as a signal for "new
  // user."
  if (recently_used_backgrounds_list.size() == 1 &&
      recently_used_backgrounds_list[0].is_bool() &&
      recently_used_backgrounds_list[0].GetBool()) {
    home_background_image_service_->FetchDefaultCollectionImages(
        base::BindOnce(&HomeBackgroundCustomizationService::
                           DefaultRecentlyUsedBackgroundsLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // recently_used_backgrounds_ is an LRU cache, so the items need to be added
  // in reverse order, so the oldest item is added first.
  for (const base::Value& background_value :
       base::Reversed(recently_used_backgrounds_list)) {
    if (background_value.is_string()) {
      recently_used_backgrounds_.Put(
          DecodeThemeSpecificsIos(background_value.GetString()));
    } else if (background_value.is_dict()) {
      std::optional<HomeUserUploadedBackground> user_background =
          HomeUserUploadedBackground::FromDict(background_value.GetDict());
      if (user_background) {
        recently_used_backgrounds_.Put(user_background.value());
        image_paths_in_use.insert(base::FilePath(user_background->image_path));
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
    recently_used_backgrounds_.Put(std::move(current_background.value()));
    StoreRecentlyUsedBackgroundsList();
  }

  // Clean up any images that failed to be deleted for any reason.
  user_image_manager_->DeleteUnusedImages(image_paths_in_use);
}

HomeBackgroundCustomizationService::~HomeBackgroundCustomizationService() {}

void HomeBackgroundCustomizationService::Shutdown() {}

void HomeBackgroundCustomizationService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kIosSavedThemeSpecificsIos,
                               std::string());
  registry->RegisterDictionaryPref(prefs::kIosUserUploadedBackground);
  // Use a simple list as a sentinel value to indicate "new user".
  registry->RegisterListPref(prefs::kIosRecentlyUsedBackgrounds,
                             base::Value::List().Append(true));
}

std::optional<HomeCustomBackground>
HomeBackgroundCustomizationService::GetCurrentCustomBackground() {
  // If customization is disabled by policy, no custom background is available.
  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return std::nullopt;
  }

  std::optional<HomeUserUploadedBackground> user_uploaded_background =
      GetCurrentUserUploadedBackground();
  if (user_uploaded_background) {
    return user_uploaded_background;
  }
  return GetCurrentNtpCustomBackground();
}

std::optional<sync_pb::NtpCustomBackground>
HomeBackgroundCustomizationService::GetCurrentNtpCustomBackground() {
  // If customization is disabled by policy, no custom background is available.
  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return std::nullopt;
  }

  if (!current_theme_.has_ntp_background()) {
    return std::nullopt;
  }
  return current_theme_.ntp_background();
}

std::optional<sync_pb::UserColorTheme>
HomeBackgroundCustomizationService::GetCurrentColorTheme() {
  // If customization is disabled by policy, no color theme is available.
  if (!pref_service_->GetBoolean(prefs::kNTPCustomBackgroundEnabledByPolicy)) {
    return std::nullopt;
  }

  // If policy theme is managed, just return that and bypass all local data.
  if (pref_service_->IsManagedPreference(themes::prefs::kPolicyThemeColor)) {
    sync_pb::UserColorTheme theme;
    theme.set_color(
        pref_service_->GetInteger(themes::prefs::kPolicyThemeColor));
    theme.set_browser_color_variant(
        sync_pb::UserColorTheme_BrowserColorVariant_TONAL_SPOT);
    return theme;
  }

  if (!current_theme_.has_user_color_theme()) {
    return std::nullopt;
  }
  return current_theme_.user_color_theme();
}

std::vector<RecentlyUsedBackground>
HomeBackgroundCustomizationService::GetRecentlyUsedBackgrounds() {
  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return {};
  }

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
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
  }

  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return;
  }

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
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
  }

  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return;
  }

  sync_pb::UserColorTheme new_color_theme;
  new_color_theme.set_color(color);
  new_color_theme.set_browser_color_variant(color_variant);

  *current_theme_.mutable_user_color_theme() = new_color_theme;
  current_theme_.clear_ntp_background();

  ClearCurrentUserUploadedBackground();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::ClearCurrentBackground() {
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
  }

  current_theme_.Clear();

  ClearCurrentUserUploadedBackground();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::DeleteRecentlyUsedBackground(
    RecentlyUsedBackground recent_background) {
  // Make sure this is not the current background.
  RecentlyUsedBackground current_background;
  std::optional<HomeCustomBackground> current_custom_background =
      GetCurrentCustomBackground();
  if (current_custom_background) {
    current_background = current_custom_background.value();
  }
  std::optional<sync_pb::UserColorTheme> current_color_theme =
      GetCurrentColorTheme();
  if (current_color_theme) {
    current_background = current_color_theme.value();
  }
  if (current_background == recent_background) {
    return;
  }

  RecentlyUsedBackgroundInternal internal_background =
      ConvertBackgroundRepresentation(recent_background);
  RecentlyUsedBackgroundsCache::iterator iterator =
      recently_used_backgrounds_.Peek(internal_background);
  if (iterator != recently_used_backgrounds_.end()) {
    recently_used_backgrounds_.Erase(iterator);
  }
  StoreRecentlyUsedBackgroundsList();
}

void HomeBackgroundCustomizationService::StoreCurrentTheme() {
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
  }

  // Recently used backgrounds list if not updated if an entreprise policy for
  // ntp customization is enabled.
  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return;
  }

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
    recently_used_backgrounds_.Put(std::move(new_recent_background.value()));
    StoreRecentlyUsedBackgroundsList();
  }
}

void HomeBackgroundCustomizationService::StoreRecentlyUsedBackgroundsList() {
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
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
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
  }
  LoadCurrentTheme();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::LoadCurrentTheme() {
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
  }
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
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
  }

  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return;
  }

  HomeUserUploadedBackground background;
  background.image_path = image_path;
  background.framing_coordinates = framing_coordinates;
  current_user_uploaded_background_ = background;

  current_theme_.clear_ntp_background();
  current_theme_.clear_user_color_theme();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::ClearCurrentUserUploadedBackground() {
  if (!IsNTPBackgroundCustomizationEnabled()) {
    return;
  }
  current_user_uploaded_background_ = std::nullopt;
}

bool HomeBackgroundCustomizationService::
    IsCustomizationDisabledOrColorManagedByPolicy() {
  return !pref_service_->GetBoolean(
             prefs::kNTPCustomBackgroundEnabledByPolicy) ||
         pref_service_->IsManagedPreference(themes::prefs::kPolicyThemeColor);
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
    return std::get<HomeUserUploadedBackground>(background);
  }
}

RecentlyUsedBackgroundInternal
HomeBackgroundCustomizationService::ConvertBackgroundRepresentation(
    RecentlyUsedBackground background) {
  if (std::holds_alternative<HomeCustomBackground>(background)) {
    HomeCustomBackground custom_background =
        std::get<HomeCustomBackground>(background);
    if (std::holds_alternative<sync_pb::NtpCustomBackground>(
            custom_background)) {
      sync_pb::NtpCustomBackground ntp_custom_background =
          std::get<sync_pb::NtpCustomBackground>(custom_background);
      sync_pb::ThemeSpecificsIos theme_specifics;
      *theme_specifics.mutable_ntp_background() = ntp_custom_background;
      return theme_specifics;
    } else {
      return std::get<HomeUserUploadedBackground>(custom_background);
    }
  } else {
    sync_pb::UserColorTheme user_color_theme =
        std::get<sync_pb::UserColorTheme>(background);
    sync_pb::ThemeSpecificsIos theme_specifics;
    *theme_specifics.mutable_user_color_theme() = user_color_theme;
    return theme_specifics;
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

void HomeBackgroundCustomizationService::DefaultRecentlyUsedBackgroundsLoaded(
    const HomeBackgroundImageService::CollectionImageMap& collection_map) {
  // Iterate backwards so the items at the end of the list are pushed into the
  // cache first, ending up at the end of the cache.
  for (const auto& [collection_name, collection_images] :
       base::Reversed(collection_map)) {
    for (const auto& image : base::Reversed(collection_images)) {
      std::string attribution_line_1;
      std::string attribution_line_2;
      // Set attribution lines if available.
      if (!image.attribution.empty()) {
        attribution_line_1 = image.attribution[0];
        if (image.attribution.size() > 1) {
          attribution_line_2 = image.attribution[1];
        }
      }

      sync_pb::NtpCustomBackground new_background;
      new_background.set_url(image.image_url.spec());
      new_background.set_attribution_line_1(attribution_line_1);
      new_background.set_attribution_line_2(attribution_line_2);
      new_background.set_attribution_action_url(
          image.attribution_action_url.spec());
      new_background.set_collection_id(image.collection_id);

      sync_pb::ThemeSpecificsIos new_theme_specifics;
      *new_theme_specifics.mutable_ntp_background() = new_background;

      recently_used_backgrounds_.Put(new_theme_specifics);
    }
  }

  StoreRecentlyUsedBackgroundsList();
}

void HomeBackgroundCustomizationService::OnPolicyPrefsChanged(
    const std::string& name) {
  CHECK(themes::prefs::kPolicyThemeColor == name ||
        prefs::kNTPCustomBackgroundEnabledByPolicy == name);

  // When policy changes, background may change, so make sure observers are
  // updated.
  NotifyObserversOfBackgroundChange();
}
