// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

#import <Foundation/Foundation.h>

#import <set>
#import <string_view>

#import "base/base64.h"
#import "base/containers/adapters.h"
#import "base/feature_list.h"
#import "base/logging.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/sync/base/features.h"
#import "components/sync/model/syncable_service.h"
#import "components/sync/protocol/theme_specifics.pb.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "components/themes/pref_names.h"
#import "ios/chrome/browser/home_customization/model/home_background_customization_service_observer.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/user_uploaded_image_manager.h"
#import "ios/chrome/browser/home_customization/utils/theme_ios_specifics_utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "third_party/skia/include/core/SkColor.h"
#import "url/gurl.h"

namespace sync_pb {

bool operator==(const sync_pb::NtpCustomBackground& lhs,
                const sync_pb::NtpCustomBackground& rhs) {
  return home_customization::AreNtpCustomBackgroundsEquivalent(lhs, rhs);
}

bool operator==(const sync_pb::UserColorTheme& lhs,
                const sync_pb::UserColorTheme& rhs) {
  return home_customization::AreUserColorThemesEquivalent(lhs, rhs);
}

bool operator==(const sync_pb::ThemeIosSpecifics& lhs,
                const sync_pb::ThemeIosSpecifics& rhs) {
  return home_customization::AreThemeIosSpecificsEquivalent(lhs, rhs);
}

}  // namespace sync_pb

namespace {

// Checks if the legacy theme pref has been migrated. If not, copies the legacy
// value to the new pref and marks migration as complete. Returns the encoded
// migrated theme if migration occurred, or `std::nullopt` otherwise.
std::optional<std::string> MigrateLegacyThemeIfNeeded(
    PrefService* profile_pref_service) {
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncThemesIos));

  if (profile_pref_service->GetBoolean(prefs::kIosNtpThemeMigrationComplete)) {
    return std::nullopt;
  }

  // Mark migration as complete immediately so it's not tried again.
  profile_pref_service->SetBoolean(prefs::kIosNtpThemeMigrationComplete, true);

  const std::string legacy_theme =
      profile_pref_service->GetString(prefs::kIosSavedThemeSpecificsIos);

  // Only migrate if legacy data exists.
  if (!legacy_theme.empty()) {
    profile_pref_service->SetString(prefs::kIosNtpThemeSpecifics, legacy_theme);
    return legacy_theme;
  }

  return std::nullopt;
}

// Retrieves the active `ThemeIosSpecifics`.
std::string GetThemeSpecifics(PrefService* profile_pref_service) {
  if (base::FeatureList::IsEnabled(syncer::kSyncThemesIos)) {
    return profile_pref_service->GetString(prefs::kIosNtpThemeSpecifics);
  }

  // When `syncer::kSyncThemesIos` is disabled use the legacy theme pref.
  return profile_pref_service->GetString(prefs::kIosSavedThemeSpecificsIos);
}

// Sets the string value for `pref_name` to `value` in `pref_service`. If
// `value` is empty, the pref is cleared instead.
void SetOrClearStringPref(PrefService* pref_service,
                          std::string_view pref_name,
                          const std::string& value) {
  if (value.empty()) {
    pref_service->ClearPref(pref_name);
  } else {
    pref_service->SetString(pref_name, value);
  }
}

// Saves the encoded theme to the appropriate pref based on sync state.
void SaveThemeSpecifics(PrefService* profile_pref_service,
                        const std::string& encoded_theme,
                        bool is_syncing) {
  // Only write to the legacy pref if the user is NOT actively syncing. (This
  // gracefully freezes the user's pre-sign-in state while sync is running.)
  if (!is_syncing) {
    SetOrClearStringPref(profile_pref_service,
                         prefs::kIosSavedThemeSpecificsIos, encoded_theme);
  }

  if (!base::FeatureList::IsEnabled(syncer::kSyncThemesIos)) {
    return;
  }

  SetOrClearStringPref(profile_pref_service, prefs::kIosNtpThemeSpecifics,
                       encoded_theme);

  // If writing a new value, ensure migration is marked complete. This prevents
  // any potential weird edge case where a user saves a theme somehow before
  // migration logic ever ran.
  if (!encoded_theme.empty() &&
      !profile_pref_service->GetBoolean(prefs::kIosNtpThemeMigrationComplete)) {
    profile_pref_service->SetBoolean(prefs::kIosNtpThemeMigrationComplete,
                                     true);
  }
}

}  // namespace

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

  CHECK(pref_service_);

  pref_change_registrar_.Init(pref_service_);
  PrefChangeRegistrar::NamedChangeCallback callback = base::BindRepeating(
      &HomeBackgroundCustomizationService::OnPolicyPrefsChanged,
      weak_ptr_factory_.GetWeakPtr());
  pref_change_registrar_.Add(themes::prefs::kPolicyThemeColor, callback);
  pref_change_registrar_.Add(prefs::kNTPCustomBackgroundEnabledByPolicy,
                             callback);

  LoadCurrentTheme();

  if (base::FeatureList::IsEnabled(syncer::kSyncThemesIos)) {
    theme_syncable_service_ = std::make_unique<ThemeSyncableServiceIOS>(this);
  }

  const base::ListValue& recently_used_backgrounds_list =
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
          DecodeThemeIosSpecifics(background_value.GetString()));
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

void HomeBackgroundCustomizationService::Shutdown() {
  // It's safe to call `reset()` unconditionally.
  theme_syncable_service_.reset();
}

sync_pb::ThemeIosSpecifics HomeBackgroundCustomizationService::GetCurrentTheme()
    const {
  return current_theme_;
}

void HomeBackgroundCustomizationService::ApplyTheme(
    const sync_pb::ThemeIosSpecifics& theme) {
  current_theme_ = theme;

  ClearCurrentUserUploadedBackground();

  StoreCurrentTheme();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::CacheLocalTheme() {
  std::string encoded_theme = EncodeThemeIosSpecifics(current_theme_);

  SetOrClearStringPref(pref_service_, prefs::kIosSavedThemeSpecificsIos,
                       encoded_theme);
}

void HomeBackgroundCustomizationService::RestoreCachedTheme() {
  std::string saved_encoded_theme =
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos);

  sync_pb::ThemeIosSpecifics cached_theme =
      DecodeThemeIosSpecifics(saved_encoded_theme);

  ApplyTheme(cached_theme);
}

bool HomeBackgroundCustomizationService::IsCurrentThemeSyncable() const {
  if (IsCurrentThemeManagedByPolicy()) {
    return false;
  }

  // If a user uploaded background is set, do NOT sync.
  return !current_user_uploaded_background_.has_value();
}

bool HomeBackgroundCustomizationService::IsCurrentThemeManagedByPolicy() const {
  return IsCustomizationDisabledOrColorManagedByPolicy();
}

void HomeBackgroundCustomizationService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kIosSavedThemeSpecificsIos,
                               std::string());
  registry->RegisterDictionaryPref(prefs::kIosUserUploadedBackground);
  // Use a simple list as a sentinel value to indicate "new user".
  registry->RegisterListPref(prefs::kIosRecentlyUsedBackgrounds,
                             base::ListValue().Append(true));
  registry->RegisterStringPref(prefs::kIosNtpThemeSpecifics, std::string());
  registry->RegisterBooleanPref(prefs::kIosNtpThemeMigrationComplete, false);
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

  // Recently used backgrounds list if not updated if an enterprise policy for
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

  std::string encoded_theme = EncodeThemeIosSpecifics(current_theme_);
  bool is_syncing =
      theme_syncable_service_ && theme_syncable_service_->IsSyncing();
  SaveThemeSpecifics(pref_service_, encoded_theme, is_syncing);

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

  base::ListValue recently_used_backgrounds_list;
  for (const RecentlyUsedBackgroundInternal& background :
       recently_used_backgrounds_) {
    if (std::holds_alternative<sync_pb::ThemeIosSpecifics>(background)) {
      sync_pb::ThemeIosSpecifics theme =
          std::get<sync_pb::ThemeIosSpecifics>(background);
      recently_used_backgrounds_list.Append(EncodeThemeIosSpecifics(theme));
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

  std::string saved_encoded_theme = GetThemeSpecifics(pref_service_);

  // If theme sync is enabled, check if a migration from legacy theme storage is
  // needed.
  if (base::FeatureList::IsEnabled(syncer::kSyncThemesIos)) {
    std::optional<std::string> migrated_theme =
        MigrateLegacyThemeIfNeeded(pref_service_);

    // Use the migrated theme if present, otherwise keep the existing
    // `saved_encoded_theme`.
    saved_encoded_theme = migrated_theme.value_or(saved_encoded_theme);
  }

  current_theme_ = DecodeThemeIosSpecifics(saved_encoded_theme);

  const base::DictValue& background_data =
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

  if (theme_syncable_service_) {
    theme_syncable_service_->OnThemeChanged();
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
    IsCustomizationDisabledOrColorManagedByPolicy() const {
  return !pref_service_->GetBoolean(
             prefs::kNTPCustomBackgroundEnabledByPolicy) ||
         pref_service_->IsManagedPreference(themes::prefs::kPolicyThemeColor);
}

syncer::SyncableService*
HomeBackgroundCustomizationService::GetThemeSyncableService() {
  if (!theme_syncable_service_) {
    return nullptr;
  }

  return theme_syncable_service_.get();
}

RecentlyUsedBackground
HomeBackgroundCustomizationService::ConvertBackgroundRepresentation(
    RecentlyUsedBackgroundInternal background) {
  if (std::holds_alternative<sync_pb::ThemeIosSpecifics>(background)) {
    sync_pb::ThemeIosSpecifics theme_specifics =
        std::get<sync_pb::ThemeIosSpecifics>(background);
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
      sync_pb::ThemeIosSpecifics theme_specifics;
      *theme_specifics.mutable_ntp_background() = ntp_custom_background;
      return theme_specifics;
    } else {
      return std::get<HomeUserUploadedBackground>(custom_background);
    }
  } else {
    sync_pb::UserColorTheme user_color_theme =
        std::get<sync_pb::UserColorTheme>(background);
    sync_pb::ThemeIosSpecifics theme_specifics;
    *theme_specifics.mutable_user_color_theme() = user_color_theme;
    return theme_specifics;
  }
}

std::string HomeBackgroundCustomizationService::EncodeThemeIosSpecifics(
    sync_pb::ThemeIosSpecifics theme_ios_specifics) {
  std::string serialized = theme_ios_specifics.SerializeAsString();
  // Encode bytestring so it can be stored in a pref.
  return base::Base64Encode(serialized);
}

sync_pb::ThemeIosSpecifics
HomeBackgroundCustomizationService::DecodeThemeIosSpecifics(
    std::string encoded) {
  // This pref is base64 encoded, so decode it first.
  std::string serialized;
  base::Base64Decode(encoded, &serialized);
  sync_pb::ThemeIosSpecifics theme_ios_specifics;
  theme_ios_specifics.ParseFromString(serialized);
  return theme_ios_specifics;
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

      sync_pb::ThemeIosSpecifics new_theme_specifics;
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
