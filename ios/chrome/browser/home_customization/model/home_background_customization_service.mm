// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/home_background_customization_service.h"

#import <Foundation/Foundation.h>

#import <set>
#import <string_view>
#import <utility>

#import "base/base64.h"
#import "base/containers/adapters.h"
#import "base/feature_list.h"
#import "base/files/file_util.h"
#import "base/json/json_reader.h"
#import "base/logging.h"
#import "base/task/thread_pool.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/image_fetcher/core/request_metadata.h"
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
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promos_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "services/network/public/mojom/fetch_api.mojom-shared.h"
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

// The number of maximum recently used backgrounds to store.
const int kMaxRecentlyUsedBackgrounds = 7;

// Sentinel collection ID used in `ThemeIosSpecifics` to represent the
// ephemeral theme in `current_theme_`.
constexpr std::string_view kEphemeralThemeCollectionId = "ephemeral_theme";

// NetworkTrafficAnnotationTag for fetching the ephemeral theme promo Lottie
// animation JSON.
const net::NetworkTrafficAnnotationTag kEphemeralPromoTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("ntp_ephemeral_theme_promo_animation",
                                        R"(
        semantics {
          sender: "NtpEphemeralThemePromo"
          description:
            "Downloads the Lottie JSON animation for the New Tab Page "
            "ephemeral theme promo configured via Finch."
          trigger:
            "Triggered once on HomeBackgroundCustomizationService startup "
            "when kNewTabPageEphemeralTheme is enabled and the promo data "
            "has not yet been cached in preferences."
          data: "None (fetches a static animation asset URL)."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is controlled by the NewTabPageEphemeralTheme "
            "feature flag."
          chrome_policy {
            NTPCustomBackgroundEnabled {
              NTPCustomBackgroundEnabled: false
            }
          }
        })");

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

// Returns true if the theme has no relevant properties set.
bool IsThemeEmpty(const sync_pb::ThemeIosSpecifics& theme) {
  return !theme.has_ntp_background() && !theme.has_user_color_theme();
}

// Creates `dir_path` if needed and writes `contents` to `file_path`.
bool WriteEphemeralThemeFile(const base::FilePath& dir_path,
                             const base::FilePath& file_path,
                             const std::string& contents) {
  return base::CreateDirectory(dir_path) &&
         base::WriteFile(file_path, contents);
}

// Parses a JSON dictionary string into a `base::DictValue`, returning an empty
// dictionary if parsing fails.
base::DictValue ParseColorMappingDict(std::string_view json_string) {
  return base::JSONReader::ReadDict(json_string,
                                    base::JSON_PARSE_CHROMIUM_EXTENSIONS)
      .value_or(base::DictValue());
}

}  // namespace

HomeBackgroundCustomizationService::HomeBackgroundCustomizationService(
    PrefService* pref_service,
    UserUploadedImageManager* user_image_manager,
    HomeBackgroundImageService* home_background_image_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::FilePath& state_path,
    PromosManager* promos_manager)
    : recently_used_backgrounds_(kMaxRecentlyUsedBackgrounds),
      pref_service_(pref_service),
      user_image_manager_(user_image_manager),
      home_background_image_service_(home_background_image_service),
      url_loader_factory_(std::move(url_loader_factory)),
      state_path_(state_path),
      promos_manager_(promos_manager),
      weak_ptr_factory_{this} {
  CHECK(pref_service_);

  pref_change_registrar_.Init(pref_service_);
  PrefChangeRegistrar::NamedChangeCallback callback = base::BindRepeating(
      &HomeBackgroundCustomizationService::OnPolicyPrefsChanged,
      weak_ptr_factory_.GetWeakPtr());
  pref_change_registrar_.Add(themes::kPolicyThemeColor, callback);
  pref_change_registrar_.Add(prefs::kNTPCustomBackgroundEnabledByPolicy,
                             callback);

  LoadCurrentTheme();
  MaybeFetchEphemeralThemeData();

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
  if (!IsCurrentEphemeralTheme() &&
      (GetCurrentNtpCustomBackground() || GetCurrentColorTheme())) {
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
  weak_ptr_factory_.InvalidateWeakPtrs();
  ephemeral_promo_url_loader_.reset();
  ephemeral_theme_image_fetcher_.reset();
  promos_manager_ = nullptr;
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
  pref_service_->ClearPref(prefs::kIosNtpThemeSpecifics);

  // If there's already a valid active user-uploaded background, don't clobber
  // it.
  const base::DictValue& active_background =
      pref_service_->GetDict(prefs::kIosUserUploadedBackground);
  if (!active_background.empty()) {
    NotifyObserversOfBackgroundChange();
    return;
  }

  std::string saved_encoded_theme =
      pref_service_->GetString(prefs::kIosSavedThemeSpecificsIos);
  sync_pb::ThemeIosSpecifics cached_theme =
      DecodeThemeIosSpecifics(saved_encoded_theme);

  // A valid cached theme exists.
  if (!IsThemeEmpty(cached_theme)) {
    ApplyTheme(cached_theme);
    return;
  }

  current_theme_ = cached_theme;

  // No cached theme exists. Conditionally fallback to a local, non-syncing
  // background if it exists.
  const base::DictValue& cached_background =
      pref_service_->GetDict(prefs::kIosCachedUserUploadedBackground);

  if (cached_background.empty()) {
    NotifyObserversOfBackgroundChange();
    return;
  }

  pref_service_->SetDict(prefs::kIosUserUploadedBackground,
                         cached_background.Clone());

  current_user_uploaded_background_ =
      HomeUserUploadedBackground::FromDict(cached_background);

  NotifyObserversOfBackgroundChange();
}

bool HomeBackgroundCustomizationService::IsCurrentThemeSyncable() const {
  if (IsCurrentThemeManagedByPolicy()) {
    return false;
  }

  // If a user uploaded background or ephemeral theme is set, do NOT sync.
  return !current_user_uploaded_background_.has_value() &&
         !IsCurrentEphemeralTheme();
}

bool HomeBackgroundCustomizationService::IsCurrentThemeManagedByPolicy() const {
  return IsCustomizationDisabledOrColorManagedByPolicy();
}

void HomeBackgroundCustomizationService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kIosSavedThemeSpecificsIos,
                               std::string());
  registry->RegisterDictionaryPref(prefs::kIosUserUploadedBackground);
  registry->RegisterDictionaryPref(prefs::kIosCachedUserUploadedBackground);
  // Use a simple list as a sentinel value to indicate "new user".
  registry->RegisterListPref(prefs::kIosRecentlyUsedBackgrounds,
                             base::ListValue().Append(true));
  registry->RegisterStringPref(prefs::kIosNtpThemeSpecifics, std::string());
  registry->RegisterBooleanPref(prefs::kIosNtpThemeMigrationComplete, false);
  registry->RegisterDictionaryPref(prefs::kIosNtpEphemeralThemeData);
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
  // If customization is disabled by policy or the ephemeral theme is active,
  // no custom background is available.
  if (IsCustomizationDisabledOrColorManagedByPolicy() ||
      IsCurrentEphemeralTheme()) {
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
  if (pref_service_->IsManagedPreference(themes::kPolicyThemeColor)) {
    sync_pb::UserColorTheme theme;
    theme.set_color(pref_service_->GetInteger(themes::kPolicyThemeColor));
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
  current_theme_.Clear();

  ClearCurrentUserUploadedBackground();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::SetCurrentEphemeralTheme(
    SkColor color,
    sync_pb::UserColorTheme::BrowserColorVariant color_variant) {
  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return;
  }

  current_theme_.Clear();
  current_theme_.mutable_ntp_background()->set_collection_id(
      std::string(kEphemeralThemeCollectionId));
  sync_pb::UserColorTheme* color_theme =
      current_theme_.mutable_user_color_theme();
  color_theme->set_color(color);
  color_theme->set_browser_color_variant(color_variant);

  ClearCurrentUserUploadedBackground();

  NotifyObserversOfBackgroundChange();
}

bool HomeBackgroundCustomizationService::IsCurrentEphemeralTheme() const {
  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return false;
  }

  return current_theme_.has_ntp_background() &&
         current_theme_.ntp_background().collection_id() ==
             kEphemeralThemeCollectionId;
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

  ClearCachedUserUploadedBackground(recent_background);
}

void HomeBackgroundCustomizationService::ClearCachedUserUploadedBackground(
    const RecentlyUsedBackground& recent_background) {
  const auto* custom_background =
      std::get_if<HomeCustomBackground>(&recent_background);
  if (!custom_background) {
    return;
  }

  const auto* user_uploaded_background =
      std::get_if<HomeUserUploadedBackground>(custom_background);
  if (!user_uploaded_background) {
    return;
  }

  const base::DictValue& cached_background =
      pref_service_->GetDict(prefs::kIosCachedUserUploadedBackground);

  std::optional<HomeUserUploadedBackground> cached_user_uploaded_background =
      HomeUserUploadedBackground::FromDict(cached_background);

  if (!cached_user_uploaded_background ||
      cached_user_uploaded_background->image_path !=
          user_uploaded_background->image_path) {
    return;
  }

  pref_service_->ClearPref(prefs::kIosCachedUserUploadedBackground);
}

void HomeBackgroundCustomizationService::StoreCurrentTheme() {
  // Recently used backgrounds list if not updated if an enterprise policy for
  // ntp customization is enabled.
  if (IsCustomizationDisabledOrColorManagedByPolicy()) {
    return;
  }

  // Only update recently used backgrounds list if the background is not
  // default or ephemeral.
  std::optional<RecentlyUsedBackgroundInternal> new_recent_background =
      std::nullopt;
  if (!IsCurrentEphemeralTheme() &&
      (GetCurrentNtpCustomBackground() || GetCurrentColorTheme())) {
    new_recent_background = current_theme_;
  }

  std::string encoded_theme = EncodeThemeIosSpecifics(current_theme_);
  bool is_syncing =
      theme_syncable_service_ && theme_syncable_service_->IsSyncing();
  SaveThemeSpecifics(pref_service_, encoded_theme, is_syncing);

  if (current_user_uploaded_background_) {
    pref_service_->SetDict(prefs::kIosUserUploadedBackground,
                           current_user_uploaded_background_->ToDict());
    pref_service_->SetDict(prefs::kIosCachedUserUploadedBackground,
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
  LoadCurrentTheme();

  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::LoadCurrentTheme() {
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
  current_user_uploaded_background_ = std::nullopt;
}

bool HomeBackgroundCustomizationService::
    IsCustomizationDisabledOrColorManagedByPolicy() const {
  return !pref_service_->GetBoolean(
             prefs::kNTPCustomBackgroundEnabledByPolicy) ||
         pref_service_->IsManagedPreference(themes::kPolicyThemeColor);
}

bool HomeBackgroundCustomizationService::IsThemeSyncActive() {
  return theme_syncable_service_ && theme_syncable_service_->IsSyncing();
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
  CHECK(themes::kPolicyThemeColor == name ||
        prefs::kNTPCustomBackgroundEnabledByPolicy == name);

  // When policy changes, background may change, so make sure observers are
  // updated.
  NotifyObserversOfBackgroundChange();
}

void HomeBackgroundCustomizationService::MaybeFetchEphemeralThemeData() {
  if (!url_loader_factory_ || state_path_.empty()) {
    return;
  }

  if (!IsNTPEphemeralThemeEnabled()) {
    return;
  }

  // Ephemeral theme data is only written to prefs once all assets have been
  // downloaded and saved to disk. If the pref is non-empty, skip
  // re-downloading.
  if (!pref_service_->GetDict(prefs::kIosNtpEphemeralThemeData).empty()) {
    return;
  }

  std::vector<EphemeralThemeAsset> assets = {
      {GURL(kNewTabPageEphemeralThemeAnimationUrlParam.Get()),
       kEphemeralThemeAnimationFileName, kEphemeralThemeAnimationPathKey,
       /*is_image=*/false},
      {GURL(kNewTabPageEphemeralThemeAnimationPromoUrlParam.Get()),
       kEphemeralThemePromoAnimationFileName,
       kEphemeralThemeAnimationPromoPathKey,
       /*is_image=*/false},
      {GURL(kNewTabPageEphemeralThemeGoogleLogoLightUrlParam.Get()),
       kEphemeralThemeGoogleLogoLightFileName,
       kEphemeralThemeGoogleLogoLightPathKey,
       /*is_image=*/true},
      {GURL(kNewTabPageEphemeralThemeGoogleLogoDarkUrlParam.Get()),
       kEphemeralThemeGoogleLogoDarkFileName,
       kEphemeralThemeGoogleLogoDarkPathKey,
       /*is_image=*/true},
  };

  // Validate all required asset URLs upfront before starting the sequential
  // download chain to avoid downloading partial assets if any URL is invalid.
  for (const EphemeralThemeAsset& asset : assets) {
    if (!asset.url.is_valid()) {
      return;
    }
  }

  FetchNextEphemeralThemeAsset(std::move(assets), base::DictValue());
}

void HomeBackgroundCustomizationService::FetchNextEphemeralThemeAsset(
    std::vector<EphemeralThemeAsset> pending_assets,
    base::DictValue theme_dict) {
  if (pending_assets.empty()) {
    theme_dict.Set(
        kEphemeralThemeAnimationColorMappingKey,
        ParseColorMappingDict(
            kNewTabPageEphemeralThemeAnimationColorMappingParam.Get()));
    theme_dict.Set(
        kEphemeralThemeAnimationPromoColorMappingKey,
        ParseColorMappingDict(
            kNewTabPageEphemeralThemeAnimationPromoColorMappingParam.Get()));
    theme_dict.Set(kEphemeralThemeSeedColorKey,
                   kNewTabPageEphemeralThemeSeedColorParam.Get());

    pref_service_->SetDict(prefs::kIosNtpEphemeralThemeData,
                           std::move(theme_dict));

    if (promos_manager_) {
      promos_manager_->RegisterPromoForSingleDisplay(
          promos_manager::Promo::EphemeralTheme);
    }
    return;
  }

  if (!url_loader_factory_) {
    return;
  }

  const EphemeralThemeAsset current_asset = pending_assets.front();
  auto download_callback = base::BindOnce(
      &HomeBackgroundCustomizationService::OnEphemeralThemeAssetDownloaded,
      weak_ptr_factory_.GetWeakPtr(), std::move(pending_assets),
      std::move(theme_dict));

  if (current_asset.is_image) {
    if (!ephemeral_theme_image_fetcher_) {
      ephemeral_theme_image_fetcher_ =
          std::make_unique<image_fetcher::ImageDataFetcher>(
              url_loader_factory_);
      ephemeral_theme_image_fetcher_->SetImageDownloadLimit(
          network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
    }
    ephemeral_theme_image_fetcher_->FetchImageData(
        current_asset.url,
        base::BindOnce([](const std::string& image_data,
                          const image_fetcher::RequestMetadata&) {
          return image_data;
        }).Then(std::move(download_callback)),
        kEphemeralPromoTrafficAnnotation);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = current_asset.url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  ephemeral_promo_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kEphemeralPromoTrafficAnnotation);
  ephemeral_promo_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce([](std::optional<std::string> response_body) {
        return response_body.value_or(std::string());
      }).Then(std::move(download_callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void HomeBackgroundCustomizationService::OnEphemeralThemeAssetDownloaded(
    std::vector<EphemeralThemeAsset> pending_assets,
    base::DictValue theme_dict,
    std::string data) {
  ephemeral_promo_url_loader_.reset();

  if (data.empty() || pending_assets.empty()) {
    return;
  }

  base::FilePath bundle_dir =
      state_path_.AppendASCII(kEphemeralThemeDirectoryName);
  base::FilePath file_path =
      bundle_dir.AppendASCII(pending_assets.front().file_name);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&WriteEphemeralThemeFile, bundle_dir, file_path,
                     std::move(data)),
      base::BindOnce(
          &HomeBackgroundCustomizationService::OnEphemeralThemeAssetSavedToDisk,
          weak_ptr_factory_.GetWeakPtr(), std::move(pending_assets),
          std::move(theme_dict), file_path));
}

void HomeBackgroundCustomizationService::OnEphemeralThemeAssetSavedToDisk(
    std::vector<EphemeralThemeAsset> pending_assets,
    base::DictValue theme_dict,
    const base::FilePath& file_path,
    bool success) {
  if (!success || pending_assets.empty()) {
    return;
  }

  theme_dict.Set(pending_assets.front().pref_key, file_path.value());
  pending_assets.erase(pending_assets.begin());

  FetchNextEphemeralThemeAsset(std::move(pending_assets),
                               std::move(theme_dict));
}
