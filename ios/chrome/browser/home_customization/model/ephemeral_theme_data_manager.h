// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_EPHEMERAL_THEME_DATA_MANAGER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_EPHEMERAL_THEME_DATA_MANAGER_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <string>
#import <string_view>
#import <vector>

#import "base/files/file_path.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "base/values.h"
#import "url/gurl.h"

enum class HomeCustomizationBackgroundStyle : NSInteger;
class PrefRegistrySimple;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

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
inline constexpr std::string_view kEphemeralThemeSeedColorKey = "seed_color";
inline constexpr std::string_view kEphemeralThemeVersionKey = "version";
inline constexpr std::string_view kPreEphemeralThemeBackgroundStyleKey =
    "pre_ephemeral_background_style";

inline constexpr std::string_view kEphemeralThemeDirectoryName =
    "ephemeral_theme";
inline constexpr std::string_view kEphemeralThemeAnimationFileName =
    "ephemeral_animation.json";
inline constexpr std::string_view kEphemeralThemePromoAnimationFileName =
    "ephemeral_promo.json";

// Manages downloading, disk persistence, cleanup, and preference storage for
// New Tab Page ephemeral theme assets (`prefs::kIosNtpEphemeralThemeData`).
class EphemeralThemeDataManager {
 public:
  EphemeralThemeDataManager(
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::FilePath& state_path);

  EphemeralThemeDataManager(const EphemeralThemeDataManager&) = delete;
  EphemeralThemeDataManager& operator=(const EphemeralThemeDataManager&) =
      delete;

  ~EphemeralThemeDataManager();

  // Registers the profile prefs associated with ephemeral theme data.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Returns whether ephemeral theme data is currently cached in
  // `prefs::kIosNtpEphemeralThemeData`.
  bool HasCachedData() const;

  // Returns the `HomeCustomizationBackgroundStyle` stored before the ephemeral
  // theme was applied, or `HomeCustomizationBackgroundStyle::kDefault` if none
  // is recorded.
  HomeCustomizationBackgroundStyle GetPreEphemeralBackgroundStyle() const;

  // Updates `kPreEphemeralThemeBackgroundStyleKey` in
  // `prefs::kIosNtpEphemeralThemeData` if cached data is present.
  void UpdatePreEphemeralBackgroundStyle(
      HomeCustomizationBackgroundStyle style);

  // Evaluates Finch parameters and asynchronously downloads the ephemeral theme
  // assets (main animation JSON and promo animation JSON) in parallel if not
  // already cached in prefs or if the configured version is newer than the
  // cached version. Once all assets are saved to disk, populates
  // `prefs::kIosNtpEphemeralThemeData` and invokes `completion`.
  void FetchEphemeralThemeData(
      HomeCustomizationBackgroundStyle current_background_style,
      base::OnceClosure completion = base::DoNothing());

  // Deletes cached ephemeral theme asset files from disk on a background
  // sequence and clears `prefs::kIosNtpEphemeralThemeData`.
  void CleanupEphemeralThemeData();

 private:
  // Metadata for a single ephemeral theme asset to download and persist.
  struct EphemeralThemeAsset {
    GURL url;
    std::string_view file_name;
    std::string_view pref_key;
  };

  // Starts the asynchronous download for `asset`.
  void FetchAsset(const EphemeralThemeAsset& asset);

  // Handles completion of downloading `asset` and writes `data` to disk on a
  // background sequence.
  void OnAssetDownloaded(const EphemeralThemeAsset& asset, std::string data);

  // Handles completion of writing `asset` to disk at `file_path` and signals
  // `all_assets_saved_barrier_`.
  void OnAssetSavedToDisk(const EphemeralThemeAsset& asset,
                          const base::FilePath& file_path,
                          bool success);

  // Invoked by `all_assets_saved_barrier_` once all parallel asset downloads
  // and disk writes have completed. Persists `pending_theme_dict_` to
  // `prefs::kIosNtpEphemeralThemeData` and runs `fetch_completion_callback_` if
  // all assets succeeded.
  void OnAllAssetsProcessed();

  // Invoked once the background task deleting partially downloaded ephemeral
  // theme files completes, resetting the in-progress fetch state.
  void OnPartialAssetsDeleted();

  // Invoked once the background task deleting cached ephemeral theme files
  // completes, clearing `prefs::kIosNtpEphemeralThemeData`.
  void OnEphemeralThemeFilesDeleted();

  // The PrefService associated with the Profile.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // URL loader factory for network requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Profile state directory path on disk.
  const base::FilePath state_path_;

  // Active URL loaders for downloading the ephemeral theme animations in
  // parallel.
  std::vector<std::unique_ptr<network::SimpleURLLoader>> url_loaders_;

  // Barrier closure tracking completion of all parallel asset downloads and
  // disk writes.
  base::RepeatingClosure all_assets_saved_barrier_;

  // Callback invoked once all ephemeral theme assets have been downloaded,
  // written to disk, and stored in preferences.
  base::OnceClosure fetch_completion_callback_;

  // Accumulates the downloaded asset file paths while parallel downloads are in
  // flight.
  base::DictValue pending_theme_dict_;

  // Background style active when `FetchEphemeralThemeData` was initiated.
  HomeCustomizationBackgroundStyle pre_ephemeral_background_style_;

  // Tracks whether all parallel asset downloads and disk writes succeeded.
  bool all_assets_succeeded_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<EphemeralThemeDataManager> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_EPHEMERAL_THEME_DATA_MANAGER_H_
