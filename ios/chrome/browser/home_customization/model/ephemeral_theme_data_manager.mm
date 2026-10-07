// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/ephemeral_theme_data_manager.h"

#import <array>
#import <optional>
#import <utility>

#import "base/barrier_closure.h"
#import "base/check.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/json/json_reader.h"
#import "base/location.h"
#import "base/task/thread_pool.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/simple_url_loader.h"
#import "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace {

// NetworkTrafficAnnotationTag for fetching the ephemeral theme promo Lottie
// animation JSON assets.
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

// Deprecated preference key from M156 storing the promo animation file path in
// `prefs::kIosNtpEphemeralThemeData`, retained in `kFilePathKeys` so cached
// promo files are deleted during cleanup.
constexpr std::string_view kDeprecatedEphemeralThemeAnimationPromoPathKey =
    "animation_promo_path";

// Preference keys storing file paths in `prefs::kIosNtpEphemeralThemeData`.
constexpr std::array<std::string_view, 2> kFilePathKeys = {
    kEphemeralThemeAnimationPathKey,
    kDeprecatedEphemeralThemeAnimationPromoPathKey,
};

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

// Extracts non-empty file paths stored under `kFilePathKeys` in
// `ephemeral_theme_data`.
std::vector<base::FilePath> ExtractSavedFilePaths(
    const base::DictValue& ephemeral_theme_data) {
  std::vector<base::FilePath> file_paths;
  for (std::string_view key : kFilePathKeys) {
    const std::string* path = ephemeral_theme_data.FindString(key);
    if (path && !path->empty()) {
      file_paths.emplace_back(*path);
    }
  }
  return file_paths;
}

// Deletes the ephemeral theme files or directories at `file_paths`.
void DeleteEphemeralThemeFiles(std::vector<base::FilePath> file_paths) {
  for (const base::FilePath& file_path : file_paths) {
    base::DeletePathRecursively(file_path);
  }
}

}  // namespace

EphemeralThemeDataManager::EphemeralThemeDataManager(
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const base::FilePath& state_path)
    : pref_service_(pref_service),
      url_loader_factory_(std::move(url_loader_factory)),
      state_path_(state_path),
      pre_ephemeral_background_style_(
          HomeCustomizationBackgroundStyle::kDefault) {
  CHECK(pref_service_);
}

EphemeralThemeDataManager::~EphemeralThemeDataManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void EphemeralThemeDataManager::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kIosNtpEphemeralThemeData);
}

bool EphemeralThemeDataManager::HasCachedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !pref_service_->GetDict(prefs::kIosNtpEphemeralThemeData).empty();
}

HomeCustomizationBackgroundStyle
EphemeralThemeDataManager::GetPreEphemeralBackgroundStyle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<int> saved_style =
      pref_service_->GetDict(prefs::kIosNtpEphemeralThemeData)
          .FindInt(kPreEphemeralThemeBackgroundStyleKey);
  return saved_style.has_value()
             ? static_cast<HomeCustomizationBackgroundStyle>(
                   saved_style.value())
             : HomeCustomizationBackgroundStyle::kDefault;
}

void EphemeralThemeDataManager::UpdatePreEphemeralBackgroundStyle(
    HomeCustomizationBackgroundStyle style) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!HasCachedData()) {
    return;
  }

  ScopedDictPrefUpdate update(pref_service_, prefs::kIosNtpEphemeralThemeData);
  update->Set(kPreEphemeralThemeBackgroundStyleKey, static_cast<int>(style));
}

void EphemeralThemeDataManager::FetchEphemeralThemeData(
    HomeCustomizationBackgroundStyle current_background_style,
    base::OnceClosure completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If a request is already in progress, drop the new request.
  if (all_assets_saved_barrier_) {
    return;
  }

  if (!url_loader_factory_ || state_path_.empty()) {
    return;
  }

  // Ephemeral theme data is only written to prefs once all assets have been
  // downloaded and saved to disk. If the pref is non-empty and the configured
  // version is not newer than the cached version, skip re-downloading.
  const base::DictValue& cached_theme_data =
      pref_service_->GetDict(prefs::kIosNtpEphemeralThemeData);
  if (!cached_theme_data.empty()) {
    int cached_version =
        cached_theme_data.FindInt(kEphemeralThemeVersionKey).value_or(0);
    if (kNewTabPageEphemeralThemeVersionParam.Get() <= cached_version) {
      return;
    }
  }

  std::vector<EphemeralThemeAsset> assets = {
      {GURL(kNewTabPageEphemeralThemeAnimationUrlParam.Get()),
       kEphemeralThemeAnimationFileName, kEphemeralThemeAnimationPathKey},
  };

  // Validate all required asset URLs upfront before starting the parallel
  // downloads to avoid downloading partial assets if any URL is invalid.
  for (const EphemeralThemeAsset& asset : assets) {
    if (!asset.url.is_valid()) {
      return;
    }
  }

  fetch_completion_callback_ = std::move(completion);
  pre_ephemeral_background_style_ =
      (current_background_style == HomeCustomizationBackgroundStyle::kEphemeral)
          ? GetPreEphemeralBackgroundStyle()
          : current_background_style;
  pending_theme_dict_.clear();
  url_loaders_.clear();
  all_assets_succeeded_ = true;

  // Use a `BarrierClosure` to ensure all async downloads and disk writes are
  // completed before persisting the theme dictionary to prefs and running the
  // completion callback.
  all_assets_saved_barrier_ = base::BarrierClosure(
      assets.size(),
      base::BindOnce(&EphemeralThemeDataManager::OnAllAssetsProcessed,
                     weak_ptr_factory_.GetWeakPtr()));

  for (const EphemeralThemeAsset& asset : assets) {
    FetchAsset(asset);
  }
}

void EphemeralThemeDataManager::CleanupEphemeralThemeData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!HasCachedData()) {
    return;
  }

  std::vector<base::FilePath> file_paths = ExtractSavedFilePaths(
      pref_service_->GetDict(prefs::kIosNtpEphemeralThemeData));
  if (!state_path_.empty()) {
    file_paths.push_back(state_path_.AppendASCII(kEphemeralThemeDirectoryName));
  }
  if (file_paths.empty()) {
    pref_service_->ClearPref(prefs::kIosNtpEphemeralThemeData);
    return;
  }

  // Delete the ephemeral theme files on a background sequence to avoid
  // blocking the main sequence on disk I/O, and clear the pref once the
  // files have been deleted.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteEphemeralThemeFiles, std::move(file_paths)),
      base::BindOnce(&EphemeralThemeDataManager::OnEphemeralThemeFilesDeleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EphemeralThemeDataManager::FetchAsset(const EphemeralThemeAsset& asset) {
  auto download_callback =
      base::BindOnce(&EphemeralThemeDataManager::OnAssetDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), asset);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = asset.url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  std::unique_ptr<network::SimpleURLLoader> url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       kEphemeralPromoTrafficAnnotation);
  network::SimpleURLLoader* raw_url_loader = url_loader.get();
  url_loaders_.push_back(std::move(url_loader));

  raw_url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce([](std::optional<std::string> response_body) {
        return response_body.value_or(std::string());
      }).Then(std::move(download_callback)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void EphemeralThemeDataManager::OnAssetDownloaded(
    const EphemeralThemeAsset& asset,
    std::string data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (data.empty()) {
    all_assets_succeeded_ = false;
    // The `BarrierClosure` must be run regardless of whether the download
    // succeeded to ensure `OnAllAssetsProcessed` is invoked once all requests
    // finish.
    all_assets_saved_barrier_.Run();
    return;
  }

  base::FilePath bundle_dir =
      state_path_.AppendASCII(kEphemeralThemeDirectoryName);
  base::FilePath file_path = bundle_dir.AppendASCII(asset.file_name);

  // Write the asset file on a background sequence to avoid blocking the main
  // sequence on disk I/O.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&WriteEphemeralThemeFile, bundle_dir, file_path,
                     std::move(data)),
      base::BindOnce(&EphemeralThemeDataManager::OnAssetSavedToDisk,
                     weak_ptr_factory_.GetWeakPtr(), asset, file_path));
}

void EphemeralThemeDataManager::OnAssetSavedToDisk(
    const EphemeralThemeAsset& asset,
    const base::FilePath& file_path,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (success) {
    pending_theme_dict_.Set(asset.pref_key, file_path.value());
  } else {
    all_assets_succeeded_ = false;
  }

  // The `BarrierClosure` must be run regardless of whether the disk write
  // succeeded to ensure `OnAllAssetsProcessed` is invoked once all tasks
  // finish.
  all_assets_saved_barrier_.Run();
}

void EphemeralThemeDataManager::OnAllAssetsProcessed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  url_loaders_.clear();

  if (!all_assets_succeeded_) {
    std::vector<base::FilePath> file_paths =
        ExtractSavedFilePaths(pending_theme_dict_);
    pending_theme_dict_.clear();
    if (!file_paths.empty()) {
      // Delete any partially saved asset files on a background sequence and
      // reset the in-progress fetch state on the main sequence once deletion
      // completes.
      base::ThreadPool::PostTaskAndReply(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&DeleteEphemeralThemeFiles, std::move(file_paths)),
          base::BindOnce(&EphemeralThemeDataManager::OnPartialAssetsDeleted,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    all_assets_saved_barrier_.Reset();
    fetch_completion_callback_.Reset();
    return;
  }

  all_assets_saved_barrier_.Reset();

  pending_theme_dict_.Set(
      kEphemeralThemeAnimationColorMappingKey,
      ParseColorMappingDict(
          kNewTabPageEphemeralThemeAnimationColorMappingParam.Get()));
  pending_theme_dict_.Set(kEphemeralThemeSeedColorKey,
                          kNewTabPageEphemeralThemeSeedColorParam.Get());
  pending_theme_dict_.Set(kEphemeralThemeVersionKey,
                          kNewTabPageEphemeralThemeVersionParam.Get());
  pending_theme_dict_.Set(kPreEphemeralThemeBackgroundStyleKey,
                          static_cast<int>(pre_ephemeral_background_style_));

  pref_service_->SetDict(prefs::kIosNtpEphemeralThemeData,
                         std::move(pending_theme_dict_));
  pending_theme_dict_.clear();

  if (fetch_completion_callback_) {
    std::move(fetch_completion_callback_).Run();
  }
}

void EphemeralThemeDataManager::OnPartialAssetsDeleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  all_assets_saved_barrier_.Reset();
  fetch_completion_callback_.Reset();
}

void EphemeralThemeDataManager::OnEphemeralThemeFilesDeleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->ClearPref(prefs::kIosNtpEphemeralThemeData);
}
