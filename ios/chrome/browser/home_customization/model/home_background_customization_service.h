// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_

#import <string>

#import "base/base64.h"
#import "base/containers/lru_cache.h"
#import "base/memory/raw_ref.h"
#import "base/observer_list.h"
#import "base/task/sequenced_task_runner.h"
#import "base/values.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/sync/protocol/theme_specifics_ios.pb.h"
#import "components/sync/protocol/theme_types.pb.h"
#import "ios/chrome/browser/home_customization/model/home_background_data.h"
#import "ios/chrome/browser/home_customization/model/home_background_image_service.h"
#import "third_party/skia/include/core/SkColor.h"

class GURL;
class HomeBackgroundCustomizationServiceObserver;
class PrefRegistrySimple;
class PrefService;
class UserUploadedImageManager;

// Type representing any custom background on the NTP.
typedef std::variant<sync_pb::NtpCustomBackground, HomeUserUploadedBackground>
    HomeCustomBackground;

// Type of the recently used backgrounds exposed externally.
typedef std::variant<HomeCustomBackground, sync_pb::UserColorTheme>
    RecentlyUsedBackground;

// Internally used type for storing recently used backgrounds.
typedef std::variant<sync_pb::ThemeSpecificsIos, HomeUserUploadedBackground>
    RecentlyUsedBackgroundInternal;

// Type of the lru cache used to store recently used backgrounds.
typedef base::HashingLRUCacheSet<RecentlyUsedBackgroundInternal>
    RecentlyUsedBackgroundsCache;

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

// std::hash specialization for sync_pb::ThemeSpecificsIos.
template <>
struct std::hash<sync_pb::ThemeSpecificsIos> {
  size_t operator()(const sync_pb::ThemeSpecificsIos& item) const {
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
    if (std::holds_alternative<sync_pb::ThemeSpecificsIos>(item)) {
      sync_pb::ThemeSpecificsIos theme =
          std::get<sync_pb::ThemeSpecificsIos>(item);
      return std::hash<sync_pb::ThemeSpecificsIos>()(theme);
    } else {
      HomeUserUploadedBackground user_background =
          std::get<HomeUserUploadedBackground>(item);

      return std::hash<HomeUserUploadedBackground>()(user_background);
    }
  }
};

}  // namespace std

// Equality operators for theme comparison.
namespace sync_pb {
bool operator==(const sync_pb::NtpCustomBackground& lhs,
                const sync_pb::NtpCustomBackground& rhs);
bool operator==(const sync_pb::UserColorTheme& lhs,
                const sync_pb::UserColorTheme& rhs);
bool operator==(const sync_pb::ThemeSpecificsIos& lhs,
                const sync_pb::ThemeSpecificsIos& rhs);
}  // namespace sync_pb

// Service for allowing customization of the Home surface background.
class HomeBackgroundCustomizationService : public KeyedService {
 public:
  explicit HomeBackgroundCustomizationService(
      PrefService* pref_service,
      UserUploadedImageManager* user_image_manager,
      HomeBackgroundImageService* home_background_image_service);

  HomeBackgroundCustomizationService(
      const HomeBackgroundCustomizationService&) = delete;
  HomeBackgroundCustomizationService& operator=(
      const HomeBackgroundCustomizationService&) = delete;

  ~HomeBackgroundCustomizationService() override;

  // KeyedService implementation:
  void Shutdown() override;

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
  bool IsCustomizationDisabledOrColorManagedByPolicy();

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

  // Backgrounds are stored on disk as either `sync_pb::ThemeSpecificsIos` or
  // `HomeUserUploadedBackground`, as those are the 2 types that have easy
  // persistence built-in. However, backgrounds are exposed to the user as
  // either HomeCustomBackground or sync_pb::UserColorTheme. These methods
  // converts between the two representations..
  RecentlyUsedBackground ConvertBackgroundRepresentation(
      RecentlyUsedBackgroundInternal background);
  RecentlyUsedBackgroundInternal ConvertBackgroundRepresentation(
      RecentlyUsedBackground background);

  // Encodes the provided theme specifics into a string for persisting to disk.
  std::string EncodeThemeSpecificsIos(
      sync_pb::ThemeSpecificsIos theme_specifics_ios);

  // Decodes a previously-encoded string into theme specifics.
  sync_pb::ThemeSpecificsIos DecodeThemeSpecificsIos(std::string string);

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

  sync_pb::ThemeSpecificsIos current_theme_;

  std::optional<HomeUserUploadedBackground> current_user_uploaded_background_;

  // In-memory store for the recently used backgrounds. LRU cache keeps the most
  // recently used/added element at the front.
  RecentlyUsedBackgroundsCache recently_used_backgrounds_;

  // The PrefService associated with the Profile.
  raw_ptr<PrefService> pref_service_;

  // Image manager used for interacting with the filesystem.
  raw_ptr<UserUploadedImageManager> user_image_manager_;

  // Service used to load lists of recently used images.
  raw_ptr<HomeBackgroundImageService> home_background_image_service_;

  // Registrar for prefs change.
  PrefChangeRegistrar pref_change_registrar_;

  base::ObserverList<HomeBackgroundCustomizationServiceObserver> observers_;

  base::WeakPtrFactory<HomeBackgroundCustomizationService> weak_ptr_factory_;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_
