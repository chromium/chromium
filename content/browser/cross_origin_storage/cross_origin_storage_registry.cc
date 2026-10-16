// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cross_origin_storage/cross_origin_storage_registry.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/is_potentially_trustworthy.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kUserDataKey[] = "cross_origin_storage_registry";

const base::FilePath::CharType kRootDirectoryName[] =
    FILE_PATH_LITERAL("CrossOriginStorage");
const base::FilePath::CharType kMetadataFileName[] =
    FILE_PATH_LITERAL("metadata.json");
const base::FilePath::CharType kBlobsDirectoryName[] =
    FILE_PATH_LITERAL("blobs");
const base::FilePath::CharType kStagingDirectoryName[] =
    FILE_PATH_LITERAL("staging");
const base::FilePath::CharType kStagingExtension[] = FILE_PATH_LITERAL(".tmp");

const char kMetadataVersionKey[] = "version";
const char kMetadataEntriesKey[] = "entries";
const char kMetadataLastAccessTimeKey[] = "last_access_time";
const char kMetadataStoringOriginsKey[] = "storing_origins";
const char kMetadataAnyOriginKey[] = "any_origin";
const char kMetadataAllowedOriginsKey[] = "allowed_origins";
const int kMetadataVersion = 1;

// TODO(crbug.com/547302825): Launch blocker: replace with a real lookup against
// the packaged Public Hash List once it ships. Until then, availability gating
// for any-origin entries is not enforced.
bool IsHashOnPublicHashList(const CrossOriginStorageHash& hash) {
  return true;
}

std::string_view AlgorithmToName(
    blink::mojom::CrossOriginStorageAlgorithm algorithm) {
  switch (algorithm) {
    case blink::mojom::CrossOriginStorageAlgorithm::kSha256:
      return "sha256";
  }
}

std::optional<blink::mojom::CrossOriginStorageAlgorithm> AlgorithmFromName(
    std::string_view name) {
  if (name == "sha256") {
    return blink::mojom::CrossOriginStorageAlgorithm::kSha256;
  }
  return std::nullopt;
}

// Returns the directory holding committed resources for `algorithm`.
base::FilePath BlobsDirectoryForAlgorithm(
    const base::FilePath& root,
    blink::mojom::CrossOriginStorageAlgorithm algorithm) {
  return root.Append(kBlobsDirectoryName)
      .AppendASCII(AlgorithmToName(algorithm));
}

}  // namespace

CrossOriginStorageOpenResult::CrossOriginStorageOpenResult() = default;

CrossOriginStorageOpenResult::CrossOriginStorageOpenResult(
    CrossOriginStorageOpenResult&&) = default;
CrossOriginStorageOpenResult& CrossOriginStorageOpenResult::operator=(
    CrossOriginStorageOpenResult&&) = default;
CrossOriginStorageOpenResult::~CrossOriginStorageOpenResult() = default;

// -----------------------------------------------------------------------
// CrossOriginStorageRegistry::StorageBackend
// -----------------------------------------------------------------------

// All disk access and all metadata for one profile's cache. Lives on a
// blocking sequence; never touched from the UI thread.
class CrossOriginStorageRegistry::StorageBackend
    : public base::ImportantFileWriter::DataSerializer {
 public:
  explicit StorageBackend(base::FilePath root_directory)
      : root_directory_(std::move(root_directory)),
        metadata_writer_(MetadataPath(),
                         base::ImportantFileWriter::kBlockCurrentSequence) {
    Initialize();
  }

  StorageBackend(const StorageBackend&) = delete;
  StorageBackend& operator=(const StorageBackend&) = delete;
  ~StorageBackend() override {
    if (metadata_writer_.HasPendingWrite()) {
      metadata_writer_.DoScheduledWrite();
    }
  }

  CrossOriginStorageOpenResult OpenForRead(CrossOriginStorageHash hash,
                                           url::Origin caller,
                                           bool update_access_time) {
    CrossOriginStorageOpenResult result;

    if (!initialized_) {
      result.status = CrossOriginStorageStatus::kOperationFailed;
      return result;
    }

    auto it = metadata_.find(hash);
    if (it == metadata_.end()) {
      result.status = CrossOriginStorageStatus::kNotFound;
      return result;
    }

    if (!HasReadAccess(it->second, caller, hash)) {
      // Per the WICG spec (`complete a read request` & §8.4 Availability
      // gating), an entry that is not disclosable to the calling origin must
      // reject with NotFoundError so callers cannot probe whether another site
      // stored the hash.
      result.status = CrossOriginStorageStatus::kNotFound;
      return result;
    }

    const base::FilePath path = BlobPath(hash);
    base::File::Info info;
    if (!base::GetFileInfo(path, &info)) {
      metadata_.erase(it);
      SaveMetadata();
      result.status = CrossOriginStorageStatus::kNotFound;
      return result;
    }

    if (update_access_time) {
      it->second.last_access_time = base::Time::Now();
      SaveMetadata();
    }

    result.status = CrossOriginStorageStatus::kSuccess;
    result.path = path;
    result.size = info.size;
    return result;
  }

  CrossOriginStorageStatus Commit(
      CrossOriginStorageHash hash,
      url::Origin caller,
      CrossOriginStorageAccessSpec access_spec,
      CrossOriginStorageStagingWriter::Result staging_result) {
    if (staging_result.status != CrossOriginStorageStatus::kSuccess) {
      if (!staging_result.path.empty()) {
        base::DeleteFile(staging_result.path);
      }
      return staging_result.status;
    }

    if (!StagingDirectory().IsParent(staging_result.path) ||
        !base::PathExists(staging_result.path)) {
      DLOG(ERROR) << "Invalid or missing COS staging file: "
                  << staging_result.path.value();
      return CrossOriginStorageStatus::kOperationFailed;
    }

    if (!initialized_) {
      base::DeleteFile(staging_result.path);
      return CrossOriginStorageStatus::kOperationFailed;
    }

    if (caller.opaque() || !caller.GetURL().SchemeIsHTTPOrHTTPS() ||
        !net::IsOriginPotentiallyTrustworthy(caller)) {
      base::DeleteFile(staging_result.path);
      return CrossOriginStorageStatus::kInvalidArgument;
    }

    // The bytes that were actually written must hash to the value the caller
    // declared up front. This is what makes the cache content-addressed and
    // safe to share across origins.
    if (!hash.MatchesDigest(staging_result.digest)) {
      base::DeleteFile(staging_result.path);
      return CrossOriginStorageStatus::kDataError;
    }

    const base::FilePath final_path = BlobPath(hash);
    if (!base::CreateDirectory(final_path.DirName())) {
      DLOG(ERROR) << "Failed to create COS blobs directory: "
                  << final_path.DirName().value();
      base::DeleteFile(staging_result.path);
      return CrossOriginStorageStatus::kOperationFailed;
    }

    if (base::PathExists(final_path)) {
      // Another origin already contributed byte-identical content, which the
      // digest check above proves. Keep the existing file and drop ours.
      base::DeleteFile(staging_result.path);
    } else {
      base::File::Error error = base::File::FILE_OK;
      if (!base::ReplaceFile(staging_result.path, final_path, &error)) {
        DLOG(ERROR) << "Failed to commit COS resource: "
                    << base::File::ErrorToString(error);
        base::DeleteFile(staging_result.path);
        return CrossOriginStorageStatus::kOperationFailed;
      }
      // Drop any orphaned metadata left behind if the blob was previously
      // removed from disk, so a new restricted write never inherits stale
      // visibility grants.
      metadata_.erase(hash);
    }

    RecordStore(hash, caller, access_spec);
    SaveMetadata();
    return CrossOriginStorageStatus::kSuccess;
  }

  base::SequenceBound<CrossOriginStorageStagingWriter> CreateStagingWriter(
      blink::mojom::CrossOriginStorageAlgorithm algorithm) {
    if (!initialized_) {
      return {};
    }
    // Initialize() already created this, but a profile directory can be
    // removed underneath a running browser.
    if (!base::CreateDirectory(StagingDirectory())) {
      DLOG(ERROR) << "Failed to create COS staging directory: "
                  << StagingDirectory().value();
      return {};
    }

    base::FilePath path =
        StagingDirectory()
            .AppendASCII(base::Uuid::GenerateRandomV4().AsLowercaseString())
            .AddExtension(kStagingExtension);
    // Opened for reading and writing so `CrossOriginStorageStagingWriter` can
    // re-hash the on-disk file from offset 0 if a caller seeks or truncates
    // mid-stream.
    base::File file(path, base::File::FLAG_CREATE_ALWAYS |
                              base::File::FLAG_READ | base::File::FLAG_WRITE);
    if (!file.IsValid()) {
      DLOG(ERROR) << "Failed to create COS staging file: " << path.value()
                  << " error: "
                  << base::File::ErrorToString(file.error_details());
      return {};
    }

    return base::SequenceBound<CrossOriginStorageStagingWriter>(
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
        algorithm, std::move(path), std::move(file), max_resource_size_bytes_);
  }

 private:
  struct EntryMetadata {
    base::Time last_access_time;

    // Origins that have stored this content. A storing origin can always read
    // back what it stored. Exact original storing origins are preserved rather
    // than simplifying to net::SchemefulSite because:
    // 1. The Public Suffix List changes over time, so simplifying to site
    //    could break if origins separate later.
    // 2. Clear-Site-Data works per origin.
    // 3. Storage limits and Cross-Origin-Storage-Allow-Origin ceilings are
    //    tracked per writer origin.
    base::flat_set<url::Origin> storing_origins;

    // Readable by any origin (subject to the Public Hash List).
    bool any_origin = false;

    // Origins explicitly named by a storing origin.
    base::flat_set<url::Origin> allowed_origins;
  };

  void Initialize() {
    if (!base::CreateDirectory(root_directory_) ||
        !base::CreateDirectory(StagingDirectory())) {
      DLOG(ERROR) << "Failed to create COS directories under "
                  << root_directory_.value();
      return;
    }

    // TODO(crbug.com/547302825): Track cumulative disk usage per storing
    // origin, enforce free-space checks, and add LRU eviction for blobs and
    // metadata_ entries (WICG § 5.1). Currently, max_resource_size_bytes_
    // limits only individual staging files.
    std::optional<int64_t> total_disk_space =
        base::SysInfo::AmountOfTotalDiskSpace(root_directory_);
    if (total_disk_space && *total_disk_space > 0) {
      max_resource_size_bytes_ = static_cast<int64_t>(
          *total_disk_space * cos_constants::kPerOriginDiskSpaceRatio);
    }

    // Anything left in staging is from a previous session that was interrupted
    // before committing; it was never readable, so drop it.
    base::FileEnumerator enumerator(StagingDirectory(), /*recursive=*/false,
                                    base::FileEnumerator::FILES);
    for (base::FilePath path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      base::DeleteFile(path);
    }

    LoadMetadata();
    SweepUnindexedBlobs();
    initialized_ = true;
  }

  void SweepUnindexedBlobs() {
    base::FileEnumerator alg_dirs(root_directory_.Append(kBlobsDirectoryName),
                                  /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
    for (base::FilePath alg_dir = alg_dirs.Next(); !alg_dir.empty();
         alg_dir = alg_dirs.Next()) {
      auto algorithm = AlgorithmFromName(alg_dir.BaseName().MaybeAsASCII());
      base::FileEnumerator blobs(alg_dir, /*recursive=*/false,
                                 base::FileEnumerator::FILES);
      for (base::FilePath blob_path = blobs.Next(); !blob_path.empty();
           blob_path = blobs.Next()) {
        std::optional<CrossOriginStorageHash> hash =
            algorithm ? CrossOriginStorageHash::Create(
                            *algorithm, blob_path.BaseName().MaybeAsASCII())
                      : std::nullopt;
        if (!hash || !metadata_.contains(*hash)) {
          base::DeleteFile(blob_path);
        }
      }
    }
  }

  base::FilePath StagingDirectory() const {
    return root_directory_.Append(kStagingDirectoryName);
  }

  base::FilePath MetadataPath() const {
    return root_directory_.Append(kMetadataFileName);
  }

  base::FilePath BlobPath(const CrossOriginStorageHash& hash) const {
    // `hash.value()` is validated lowercase hex, so it is a safe component.
    return BlobsDirectoryForAlgorithm(root_directory_, hash.algorithm())
        .AppendASCII(hash.value());
  }

  bool HasReadAccess(const EntryMetadata& metadata,
                     const url::Origin& caller,
                     const CrossOriginStorageHash& hash) const {
    if (caller.opaque()) {
      return false;
    }
    if (metadata.storing_origins.contains(caller)) {
      return true;
    }
    const net::SchemefulSite caller_site(caller);
    for (const url::Origin& storing_origin : metadata.storing_origins) {
      if (caller_site == net::SchemefulSite(storing_origin)) {
        return true;
      }
    }
    for (const url::Origin& allowed_origin : metadata.allowed_origins) {
      if (caller_site == net::SchemefulSite(allowed_origin)) {
        return true;
      }
    }
    if (metadata.any_origin) {
      return IsHashOnPublicHashList(hash);
    }
    return false;
  }

  void RecordStore(const CrossOriginStorageHash& hash,
                   const url::Origin& caller,
                   const CrossOriginStorageAccessSpec& access_spec) {
    EntryMetadata& metadata = metadata_[hash];
    metadata.last_access_time = base::Time::Now();

    if (!caller.opaque()) {
      metadata.storing_origins.insert(caller);
    }

    // Sharing scope only ever widens: `any_origin` and `allowed_origins` are
    // independent, additive grants, and `allowed_origins` gives listed origins
    // read access even when the hash is not on the Public Hash List.
    if (access_spec.any_origin()) {
      metadata.any_origin = true;
      return;
    }
    for (const url::Origin& origin : access_spec.allowed_origins()) {
      if (metadata.allowed_origins.contains(origin)) {
        continue;
      }
      if (metadata.allowed_origins.size() >=
          cos_constants::kMaxTargetOriginsLength) {
        // TODO(crbug.com/547302825): Surface a truncation signal from Commit()
        // so CrossOriginStorageManagerImpl can log a DevTools console warning
        // when target origins exceed capacity (WICG § 3.5, § 5.1).
        DLOG(WARNING)
            << "COS entry allowed_origins list is at capacity; dropping "
               "remaining target origins.";
        break;
      }
      metadata.allowed_origins.insert(origin);
    }
  }

  void LoadMetadata() {
    const base::FilePath path = MetadataPath();
    if (!base::PathExists(path)) {
      return;
    }

    JSONFileValueDeserializer deserializer(path);
    int error_code = 0;
    std::string error_message;
    std::unique_ptr<base::Value> root =
        deserializer.Deserialize(&error_code, &error_message);
    if (!root || !root->is_dict()) {
      DLOG(ERROR) << "Failed to read COS metadata: " << error_message;
      return;
    }

    const base::DictValue& dict = root->GetDict();
    if (dict.FindInt(kMetadataVersionKey).value_or(0) != kMetadataVersion) {
      return;
    }
    const base::DictValue* entries = dict.FindDict(kMetadataEntriesKey);
    if (!entries) {
      return;
    }

    for (const auto [algorithm_name, algorithm_entries] : *entries) {
      std::optional<blink::mojom::CrossOriginStorageAlgorithm> algorithm =
          AlgorithmFromName(algorithm_name);
      if (!algorithm || !algorithm_entries.is_dict()) {
        continue;
      }

      for (const auto [hex, value] : algorithm_entries.GetDict()) {
        std::optional<CrossOriginStorageHash> hash =
            CrossOriginStorageHash::Create(*algorithm, hex);
        if (!hash || !value.is_dict()) {
          continue;
        }
        const base::DictValue& entry_dict = value.GetDict();

        EntryMetadata metadata;
        const std::string* time_string =
            entry_dict.FindString(kMetadataLastAccessTimeKey);
        int64_t microseconds = 0;
        if (time_string && base::StringToInt64(*time_string, &microseconds)) {
          metadata.last_access_time = base::Time::FromDeltaSinceWindowsEpoch(
              base::Microseconds(microseconds));
        }

        metadata.any_origin =
            entry_dict.FindBool(kMetadataAnyOriginKey).value_or(false);
        ReadOriginList(entry_dict.FindList(kMetadataStoringOriginsKey),
                       /*max_length=*/0, metadata.storing_origins);
        ReadOriginList(entry_dict.FindList(kMetadataAllowedOriginsKey),
                       cos_constants::kMaxTargetOriginsLength,
                       metadata.allowed_origins);

        metadata_.emplace(std::move(*hash), std::move(metadata));
      }
    }
  }

  // Populates `out` with unique potentially trustworthy http(s) origins from
  // `list`. When `max_length` is non-zero, stops once `out` reaches
  // `max_length` elements (`0` means unlimited).
  static void ReadOriginList(const base::ListValue* list,
                             size_t max_length,
                             base::flat_set<url::Origin>& out) {
    if (!list) {
      return;
    }
    for (const base::Value& value : *list) {
      if (max_length > 0 && out.size() >= max_length) {
        break;
      }
      if (!value.is_string()) {
        continue;
      }
      GURL gurl(value.GetString());
      if (!gurl.is_valid() || !gurl.SchemeIsHTTPOrHTTPS()) {
        continue;
      }
      url::Origin origin = url::Origin::Create(gurl);
      if (net::IsOriginPotentiallyTrustworthy(origin)) {
        out.insert(std::move(origin));
      }
    }
  }

  static base::ListValue ToValueList(
      const base::flat_set<url::Origin>& origins) {
    base::ListValue list;
    for (const url::Origin& origin : origins) {
      list.Append(origin.Serialize());
    }
    return list;
  }

  void SaveMetadata() { metadata_writer_.ScheduleWrite(this); }

  // base::ImportantFileWriter::DataSerializer:
  std::optional<std::string> SerializeData() override {
    base::DictValue entries;
    for (const auto& [hash, metadata] : metadata_) {
      base::DictValue entry;
      entry.Set(kMetadataLastAccessTimeKey,
                base::NumberToString(
                    metadata.last_access_time.ToDeltaSinceWindowsEpoch()
                        .InMicroseconds()));
      entry.Set(kMetadataStoringOriginsKey,
                ToValueList(metadata.storing_origins));
      entry.Set(kMetadataAnyOriginKey, metadata.any_origin);
      entry.Set(kMetadataAllowedOriginsKey,
                ToValueList(metadata.allowed_origins));

      base::DictValue* algorithm_entries =
          entries.EnsureDict(AlgorithmToName(hash.algorithm()));
      algorithm_entries->Set(hash.value(), std::move(entry));
    }

    base::DictValue root;
    root.Set(kMetadataVersionKey, kMetadataVersion);
    root.Set(kMetadataEntriesKey, std::move(entries));

    std::optional<std::string> json = base::WriteJson(root);
    if (!json) {
      DLOG(ERROR) << "Failed to serialize COS metadata.";
    }
    return json;
  }

  const base::FilePath root_directory_;
  base::ImportantFileWriter metadata_writer_;
  int64_t max_resource_size_bytes_ = cos_constants::kMaxResourceSizeBytes;
  bool initialized_ = false;

  std::map<CrossOriginStorageHash, EntryMetadata> metadata_;
};

// -----------------------------------------------------------------------
// CrossOriginStorageRegistry
// -----------------------------------------------------------------------

// static
CrossOriginStorageRegistry*
CrossOriginStorageRegistry::GetOrCreateForBrowserContext(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(browser_context);

  auto* existing = static_cast<CrossOriginStorageRegistry*>(
      browser_context->GetUserData(kUserDataKey));
  if (existing) {
    return existing;
  }

  base::FilePath profile_path = browser_context->GetPath();
  if (browser_context->IsOffTheRecord() || profile_path.empty()) {
    // Off-the-record profiles return their parent regular profile's directory
    // from GetPath(). Returning nullptr avoids sharing the regular profile
    // cache, leaking browsing state, or creating an Incognito mode oracle.
    // TODO(crbug.com/547302825): Support an isolated Incognito COS seeded with
    // common resources observed across the fleet.
    // TODO(crbug.com/547302825): In-memory StoragePartitions (e.g.
    // non-persistent <webview> partitions) should not share this on-disk store.
    // Reject or return nullptr for in-memory partitions when exposing this to
    // frames.
    return nullptr;
  }

  auto registry = std::make_unique<CrossOriginStorageRegistry>(
      profile_path.Append(kRootDirectoryName));
  auto* raw_registry = registry.get();
  browser_context->SetUserData(kUserDataKey, std::move(registry));
  return raw_registry;
}

CrossOriginStorageRegistry::CrossOriginStorageRegistry(
    base::FilePath root_directory)
    : backend_(base::ThreadPool::CreateSequencedTaskRunner(
                   {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                    base::TaskShutdownBehavior::BLOCK_SHUTDOWN}),
               std::move(root_directory)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CrossOriginStorageRegistry::~CrossOriginStorageRegistry() = default;

void CrossOriginStorageRegistry::OpenForRead(const CrossOriginStorageHash& hash,
                                             const url::Origin& caller,
                                             bool update_access_time,
                                             OpenForReadCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  backend_.AsyncCall(&StorageBackend::OpenForRead)
      .WithArgs(hash, caller, update_access_time)
      .Then(std::move(callback));
}

void CrossOriginStorageRegistry::Commit(
    const CrossOriginStorageHash& hash,
    const url::Origin& caller,
    CrossOriginStorageAccessSpec access_spec,
    CrossOriginStorageStagingWriter::Result staging_result,
    CommitCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  backend_.AsyncCall(&StorageBackend::Commit)
      .WithArgs(hash, caller, std::move(access_spec), std::move(staging_result))
      .Then(std::move(callback));
}

void CrossOriginStorageRegistry::CreateStagingWriter(
    blink::mojom::CrossOriginStorageAlgorithm algorithm,
    CreateStagingWriterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  backend_.AsyncCall(&StorageBackend::CreateStagingWriter)
      .WithArgs(algorithm)
      .Then(std::move(callback));
}

base::WeakPtr<CrossOriginStorageRegistry>
CrossOriginStorageRegistry::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_factory_.GetWeakPtr();
}

void CrossOriginStorageRegistry::SynchronouslyResetBackendForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  backend_.SynchronouslyResetForTest();  // IN-TEST
}

}  // namespace content
