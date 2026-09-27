// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_

#import <Foundation/Foundation.h>

#import <string>
#import <string_view>
#import <variant>
#import <vector>

#import "base/base64.h"
#import "base/containers/hashing_lru_cache.h"
#import "base/files/file_path.h"
#import "base/memory/raw_ref.h"
#import "base/memory/scoped_refptr.h"
#import "base/observer_list.h"
#import "base/task/sequenced_task_runner.h"
#import "base/values.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/sync/protocol/theme_ios_specifics.pb.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "ios/chrome/browser/home_customization/model/theme_syncable_service_ios.h"
#import "third_party/skia/include/core/SkColor.h"
#import "url/gurl.h"

class HomeBackgroundCustomizationServiceObserver;
enum class HomeCustomizationBackgroundStyle : NSInteger;
class PrefRegistrySimple;
class PrefService;
class PromosManager;
class UserUploadedImageManager;

namespace image_fetcher {
class ImageDataFetcher;
}  // namespace image_fetcher

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace syncer {
class SyncableService;
}  // namespace syncer

// Type representing any custom background on the NTP.
typedef std::variant<sync_pb::NtpCustomBackground, HomeUserUploadedBackground>
    HomeCustomBackground;

// Type of the recently used backgrounds exposed externally.
typedef std::variant<HomeCustomBackground, sync_pb::UserColorTheme>
    RecentlyUsedBackground;

// Internally used type for storing recently used backgrounds.
typedef std::variant<sync_pb::ThemeIosSpecifics, HomeUserUploadedBackground>
    RecentlyUsedBackgroundInternal;

namespace std {

// std::hash specialization for sync_pb::NtpCustomBackground.
template <>
struct std::hash<sync_pb::NtpCustomBackground> {
  size_t operator()(const sync_pb::NtpCustomBackground& item) const {
    // Only compare url from background.
    return std::hash<std::string>()(item.url());
  }
};

// std::hash specialization for sync_pb::UserColorTheme.
template <>
struct std::hash<sync_pb::UserColorTheme> {
  size_t operator()(const sync_pb::UserColorTheme& item) const {
    return std::hash<uint32_t>()(item.color()) ^
           std::hash<sync_pb::UserColorTheme::BrowserColorVariant>()(
               item.browser_color_variant());
  }
};

// std::hash specialization for sync_pb::ThemeIosSpecifics.
template <>
struct std::hash<sync_pb::ThemeIosSpecifics> {
  size_t operator()(const sync_pb::ThemeIosSpecifics& item) const {
    // Ntp Background field takes precedence. Only compare colors if theme lacks
    // a background.
    if (item.has_ntp_background()) {
      return std::hash<sync_pb::NtpCustomBackground>()(item.ntp_background());
    }

    return std::hash<sync_pb::UserColorTheme>()(item.user_color_theme());
  }
};

// std::hash specialization for HomeUserUploadedBackground.
template <>
struct std::hash<HomeUserUploadedBackground> {
  size_t operator()(const HomeUserUploadedBackground& item) const {
    return std::hash<std::string>()(item.image_path);
  }
};

// std::hash specialization for RecentlyUsedBackgroundInternal.
template <>
struct std::hash<RecentlyUsedBackgroundInternal> {
  size_t operator()(const RecentlyUsedBackgroundInternal& item) const {
    if (std::holds_alternative<sync_pb::ThemeIosSpecifics>(item)) {
      sync_pb::ThemeIosSpecifics theme =
          std::get<sync_pb::ThemeIosSpecifics>(item);
      return std::hash<sync_pb::ThemeIosSpecifics>()(theme);
    } else {
      HomeUserUploadedBackground user_background =
          std::get<HomeUserUploadedBackground>(item);

      return std::hash<HomeUserUploadedBackground>()(user_background);
    }
  }
};

}  // namespace std

// Type of the lru cache used to store recently used backgrounds.
// This needs to go after the std::hash specializations above in order to
// compile.
typedef base::HashingLRUCacheSet<RecentlyUsedBackgroundInternal>
    RecentlyUsedBackgroundsCache;

// Equality operators for theme comparison.
namespace sync_pb {
bool operator==(const sync_pb::NtpCustomBackground& lhs,
                const sync_pb::NtpCustomBackground& rhs);
bool operator==(const sync_pb::UserColorTheme& lhs,
                const sync_pb::UserColorTheme& rhs);
bool operator==(const sync_pb::ThemeIosSpecifics& lhs,
                const sync_pb::ThemeIosSpecifics& rhs);
}  // namespace sync_pb

// Preference dictionary keys, subdirectory name, and filenames for
// `prefs::kIosNtpEphemeralThemeData`.
inline constexpr std::string_view kEphemeralThemeAnimationPathKey =
    "animation_path";
inline constexpr std::string_view kEphemeralThemeAnimationColorMappingKey =
    "animation_colormapping";
inline constexpr std::string_view kEphemeralThemeAnimationPromoPathKey =
    "animation_promo_path";
inline constexpr std::string_view kEphemeralThemeAnimationPromoColorMappingKey =
    "animation_promo_colormapping";
inline constexpr std::string_view kEphemeralThemeGoogleLogoLightPathKey =
    "google_logo_light_path";
inline constexpr std::string_view kEphemeralThemeGoogleLogoDarkPathKey =
    "google_logo_dark_path";
inline constexpr std::string_view kEphemeralThemeSeedColorKey = "seed_color";
inline constexpr std::string_view kPreEphemeralThemeBackgroundStyleKey =
    "pre_ephemeral_background_style";

inline constexpr std::string_view kEphemeralThemeDirectoryName =
    "ephemeral_theme";
inline constexpr std::string_view kEphemeralThemeAnimationFileName =
    "ephemeral_animation.json";
inline constexpr std::string_view kEphemeralThemePromoAnimationFileName =
    "ephemeral_promo.json";
inline constexpr std::string_view kEphemeralThemeGoogleLogoLightFileName =
    "ephemeral_google_logo_light.png";
inline constexpr std::string_view kEphemeralThemeGoogleLogoDarkFileName =
    "ephemeral_google_logo_dark.png";

// Service for allowing customization of the Home surface background.
class HomeBackgroundCustomizationService
    : public KeyedService,
      public ThemeSyncableServiceIOS::Delegate {
 public:
  HomeBackgroundCustomizationService(
      PrefService* pref_service,
      UserUploadedImageManager* user_image_manager,
      HomeBackgroundImageService* home_background_image_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& state_path,
      PromosManager* promos_manager = nullptr);

  HomeBackgroundCustomizationService(
      const HomeBackgroundCustomizationService&) = delete;
  HomeBackgroundCustomizationService& operator=(
      const HomeBackgroundCustomizationService&) = delete;

  ~HomeBackgroundCustomizationService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // `ThemeSyncableServiceIOS::Delegate` overrides.
  sync_pb::ThemeIosSpecifics GetCurrentTheme() const override;
  void ApplyTheme(const sync_pb::ThemeIosSpecifics& theme) override;
  void CacheLocalTheme() override;
  void RestoreCachedTheme() override;
  bool IsCurrentThemeSyncable() const override;
  bool IsCurrentThemeManagedByPolicy() const override;

  // Returns the current custom background data, if there is one.
  std::optional<HomeCustomBackground> GetCurrentCustomBackground();

  // Returns the current New Tab Page color theme, if there is one.
  std::optional<sync_pb::UserColorTheme> GetCurrentColorTheme();

  // Returns a list of the recently used backgrounds.
  std::vector<RecentlyUsedBackground> GetRecentlyUsedBackgrounds();

  /// Sets the current background to the given parameters without persisting
  /// this change to disk. This represents a background image url from the
  /// NtpBackgroundService.
  /// - `background_url` is the URL of the background itself.
  /// - `thumbnail_url` is the URL of the preview thumbnail.
  /// - `attribution_line_1` is the first line of attribution for the author of
  /// the image.
  /// - `attribution_line_1` is the second line of attribution for the author of
  /// the image.
  /// - `action_url` is an action that can be taken for the attribution (e.g.
  /// visit the artist's webpage).
  /// - `collection_id` is the id of the collection the image comes from.
  void SetCurrentBackground(const GURL& background_url,
                            const GURL& thumbnail_url,
                            const std::string& attribution_line_1,
                            const std::string& attribution_line_2,
                            const GURL& attribution_action_url,
                            const std::string& collection_id);

  // Sets the current background color to the given parameters without
  // persisting this change to disk.
  void SetBackgroundColor(
      SkColor color,
      sync_pb::UserColorTheme::BrowserColorVariant color_variant);

  /// Sets the current background to a user-uploaded photo without persisting
  /// this change to disk.
  /// - `image_path` is the file path to the saved image in the profile
  /// directory.
  /// - `framing_data` contains the coordinates for how the image should be
  /// framed.
  void SetCurrentUserUploadedBackground(
      const std::string& image_path,
      const FramingCoordinates& framing_coordinates);

  // Resets the current background to the default/no changes without persisting
  // this change to disk.
  void ClearCurrentBackground();

  // Sets the current background to the ephemeral theme without persisting this
  // change to disk or adding it to the recently used backgrounds list.
  void SetCurrentEphemeralTheme(
      SkColor color,
      sync_pb::UserColorTheme::BrowserColorVariant color_variant);

  // Returns whether the ephemeral theme is currently active.
  bool IsCurrentEphemeralTheme() const;

  // Stores the current theme to disk.
  void StoreCurrentTheme();

  // Reloads the theme from disk and restores it as the current NTP
  // background.
  void RestoreCurrentTheme();

  // Deletes the recently used background from the stored list.
  void DeleteRecentlyUsedBackground(RecentlyUsedBackground recent_background);

  // Adds/Removes HomeBackgroundCustomizationServiceObserver observers.
  void AddObserver(HomeBackgroundCustomizationServiceObserver* observer);
  void RemoveObserver(HomeBackgroundCustomizationServiceObserver* observer);

  // Registers the profile prefs associated with this service.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Clears the current user-uploaded background without persisting this change
  // to disk.
  void ClearCurrentUserUploadedBackground();

  // Return whether the NTP custom background is disabled by enterprise policy.
  bool IsCustomizationDisabledOrColorManagedByPolicy() const;

  // Returns whether theme sync is actively running.
  bool IsThemeSyncActive();

  // Returns the `SyncableService` associated with `THEMES_IOS`. Returns
  // `nullptr` if the feature is disabled.
  syncer::SyncableService* GetThemeSyncableService();

 private:
  // Alerts observers when the background changes.
  void NotifyObserversOfBackgroundChange();

  // Loads the theme data from disk.
  void LoadCurrentTheme();

  // Stores the recently used backgrounds list to disk.
  void StoreRecentlyUsedBackgroundsList();

  // Extracts the current custom background from the current theme, if there is
  // one.
  std::optional<sync_pb::NtpCustomBackground> GetCurrentNtpCustomBackground();

  // Gets the current user-uploaded background data, if there is one.
  std::optional<HomeUserUploadedBackground> GetCurrentUserUploadedBackground();

  // Backgrounds are stored on disk as either `sync_pb::ThemeIosSpecifics` or
  // `HomeUserUploadedBackground`, as those are the 2 types that have easy
  // persistence built-in. However, backgrounds are exposed to the user as
  // either HomeCustomBackground or sync_pb::UserColorTheme. These methods
  // converts between the two representations..
  RecentlyUsedBackground ConvertBackgroundRepresentation(
      RecentlyUsedBackgroundInternal background);
  RecentlyUsedBackgroundInternal ConvertBackgroundRepresentation(
      RecentlyUsedBackground background);

  // Encodes the provided theme specifics into a string for persisting to disk.
  std::string EncodeThemeIosSpecifics(
      sync_pb::ThemeIosSpecifics theme_ios_specifics);

  // Decodes a previously-encoded string into theme specifics.
  sync_pb::ThemeIosSpecifics DecodeThemeIosSpecifics(std::string string);

  // Deletes the listed image from disk.
  void DeleteUserBackgroundImage(
      HomeUserUploadedBackground user_background_image);

  // Observes changes to enterprise policy prefs for theme color
  // (kPolicyThemeColor) and custom backgrounds
  // (kNTPCustomBackgroundEnabledByPolicy).
  void OnPolicyPrefsChanged(const std::string& name);

  // Handles the loaded images.
  void DefaultRecentlyUsedBackgroundsLoaded(
      const HomeBackgroundImageService::CollectionImageMap& collection_map);

  // Conditionally clears the cached user-uploaded background if
  // `recent_background` matches it.
  void ClearCachedUserUploadedBackground(
      const RecentlyUsedBackground& recent_background);

  // Metadata for a single ephemeral theme asset to download and persist.
  struct EphemeralThemeAsset {
    GURL url;
    std::string_view file_name;
    std::string_view pref_key;
    bool is_image = false;
  };

  // Returns the `HomeCustomizationBackgroundStyle` corresponding to the
  // currently active background.
  HomeCustomizationBackgroundStyle GetCurrentBackgroundStyle();

  // Restores the most recently used background, or clears the current
  // background to the default if no recently used backgrounds exist.
  void RestoreMostRecentBackground();

  // Restores the previous non-ephemeral background if the ephemeral theme is
  // currently active, deletes cached ephemeral theme asset files from disk, and
  // clears `prefs::kIosNtpEphemeralThemeData`.
  void CleanupEphemeralThemeData();

  // Evaluates Finch parameters and fetches the ephemeral theme data (main
  // animation JSON, promo animation JSON, light Google logo, and dark Google
  // logo) sequentially if not already cached in prefs.
  void MaybeFetchEphemeralThemeData();

  // Downloads the next asset in `pending_assets`, or persists `theme_dict` to
  // `prefs::kIosNtpEphemeralThemeData` and registers the promo once all assets
  // have been saved.
  void FetchNextEphemeralThemeAsset(
      std::vector<EphemeralThemeAsset> pending_assets,
      base::DictValue theme_dict);

  // Handles completion of downloading the current asset in `pending_assets` and
  // writes `data` to disk.
  void OnEphemeralThemeAssetDownloaded(
      std::vector<EphemeralThemeAsset> pending_assets,
      base::DictValue theme_dict,
      std::string data);

  // Handles completion of writing the current asset in `pending_assets` to disk
  // at `file_path` and advances to the next asset.
  void OnEphemeralThemeAssetSavedToDisk(
      std::vector<EphemeralThemeAsset> pending_assets,
      base::DictValue theme_dict,
      const base::FilePath& file_path,
      bool success);

  sync_pb::ThemeIosSpecifics current_theme_;

  std::optional<HomeUserUploadedBackground> current_user_uploaded_background_;

  // In-memory store for the recently used backgrounds. LRU cache keeps the most
  // recently used/added element at the front.
  RecentlyUsedBackgroundsCache recently_used_backgrounds_;

  // The PrefService associated with the Profile.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Image manager used for interacting with the filesystem.
  raw_ptr<UserUploadedImageManager> user_image_manager_ = nullptr;

  // Service used to load lists of recently used images.
  raw_ptr<HomeBackgroundImageService> home_background_image_service_ = nullptr;

  // URL loader factory for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Active URL loader for downloading the ephemeral theme animations.
  std::unique_ptr<network::SimpleURLLoader> ephemeral_promo_url_loader_;

  // Image fetcher for downloading the ephemeral theme Google logo images.
  std::unique_ptr<image_fetcher::ImageDataFetcher>
      ephemeral_theme_image_fetcher_;

  // Profile state directory path on disk.
  base::FilePath state_path_;

  // Promos manager used to register the ephemeral theme promo.
  raw_ptr<PromosManager> promos_manager_ = nullptr;

  // The service responsible for syncing theme data. This is null if the
  // `kSyncThemesIos` feature is disabled.
  std::unique_ptr<ThemeSyncableServiceIOS> theme_syncable_service_;

  // Registrar for prefs change.
  PrefChangeRegistrar pref_change_registrar_;

  base::ObserverList<HomeBackgroundCustomizationServiceObserver> observers_;

  base::WeakPtrFactory<HomeBackgroundCustomizationService> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_
