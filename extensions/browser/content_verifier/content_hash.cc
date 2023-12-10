// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_hash.h"

#include <set>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_hash_fetcher.h"
#include "extensions/browser/content_hash_tree.h"
#include "extensions/browser/content_verifier/content_verifier_utils.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/file_util.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace extensions {

namespace {

bool CreateDirAndWriteFile(const base::FilePath& destination,
                           const std::string& content) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  base::FilePath dir = destination.DirName();
  if (!base::CreateDirectory(dir))
    return false;

  return base::WriteFile(destination, content);
}

std::unique_ptr<VerifiedContents> ReadVerifiedContents(
    const ContentHash::FetchKey& key,
    bool delete_invalid_file) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  base::FilePath verified_contents_path =
      file_util::GetVerifiedContentsPath(key.extension_root);
  std::unique_ptr<VerifiedContents> verified_contents =
      VerifiedContents::CreateFromFile(key.verifier_key,
                                       verified_contents_path);
  if (!verified_contents ||
      verified_contents->extension_id() != key.extension_id ||
      verified_contents->version() != key.extension_version) {
    if (delete_invalid_file && !base::DeleteFile(verified_contents_path)) {
      LOG(WARNING) << "Failed to delete " << verified_contents_path.value();
    }
    return nullptr;
  }
  return verified_contents;
}

}  // namespace

ContentHash::FetchKey::FetchKey(
    const ExtensionId& extension_id,
    const base::FilePath& extension_root,
    const base::Version& extension_version,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        url_loader_factory_remote,
    const GURL& fetch_url,
    ContentVerifierKey verifier_key)
    : extension_id(extension_id),
      extension_root(extension_root),
      extension_version(extension_version),
      url_loader_factory_remote(std::move(url_loader_factory_remote)),
      fetch_url(std::move(fetch_url)),
      verifier_key(verifier_key) {}

ContentHash::FetchKey::~FetchKey() = default;

ContentHash::FetchKey::FetchKey(ContentHash::FetchKey&& other) = default;

ContentHash::FetchKey& ContentHash::FetchKey::operator=(
    ContentHash::FetchKey&& other) = default;

// static
void ContentHash::Create(
    FetchKey key,
    ContentVerifierDelegate::VerifierSourceType source_type,
    const IsCancelledCallback& is_cancelled,
    CreatedCallback created_callback) {
  if (source_type ==
      ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES) {
    // In case of signed hashes, we should read or fetch verified_contents.json
    // before moving on to work with computed_hashes.json.
    GetVerifiedContents(
        std::move(key), source_type, is_cancelled,
        base::BindOnce(&ContentHash::GetComputedHashes, source_type,
                       is_cancelled, std::move(created_callback)));
  } else {
    DCHECK_EQ(source_type,
              ContentVerifierDelegate::VerifierSourceType::UNSIGNED_HASHES);

    GetComputedHashes(source_type, is_cancelled, std::move(created_callback),
                      std::move(key), /*verified_contents=*/nullptr,
                      /*did_attempt_fetch=*/false, /*fetch_error=*/net::OK);
  }
}

void ContentHash::ForceBuildComputedHashes(
    const IsCancelledCallback& is_cancelled,
    CreatedCallback created_callback) {
  BuildComputedHashes(false /* did_fetch_verified_contents */,
                      true /* force_build */, is_cancelled);
  std::move(created_callback).Run(this, is_cancelled && is_cancelled.Run());
}

ContentHash::TreeHashVerificationResult ContentHash::VerifyTreeHashRoot(
    const base::FilePath& relative_path,
    const std::string* root) const {
  DCHECK(verified_contents_ ||
         source_type_ ==
             ContentVerifierDelegate::VerifierSourceType::UNSIGNED_HASHES);

  // If verified_contents.json is acceptably missing, provided data from
  // computed_hashes.json is the source of truth for hashes.
  if (!verified_contents_) {
    return root ? TreeHashVerificationResult::SUCCESS
                : TreeHashVerificationResult::NO_ENTRY;
  }

  if (!verified_contents_->HasTreeHashRoot(relative_path))
    return TreeHashVerificationResult::NO_ENTRY;

  if (!root || !verified_contents_->TreeHashRootEquals(relative_path, *root))
    return TreeHashVerificationResult::HASH_MISMATCH;

  return TreeHashVerificationResult::SUCCESS;
}

const ComputedHashes& ContentHash::computed_hashes() const {
  DCHECK(succeeded() && computed_hashes_);
  return *computed_hashes_;
}

// static
std::string ContentHash::ComputeTreeHashForContent(const std::string& contents,
                                                   int block_size) {
  std::vector<std::string> hashes =
      ComputedHashes::GetHashesForContent(contents, block_size);
  return ComputeTreeHashRoot(hashes, block_size / crypto::kSHA256Length);
}

ContentHash::ContentHash(
    const ExtensionId& id,
    const base::FilePath& root,
    ContentVerifierDelegate::VerifierSourceType source_type,
    std::unique_ptr<const VerifiedContents> verified_contents)
    : extension_id_(id),
      extension_root_(root),
      source_type_(source_type),
      verified_contents_(std::move(verified_contents)) {}

ContentHash::~ContentHash() = default;

// static
void ContentHash::GetVerifiedContents(
    FetchKey key,
    ContentVerifierDelegate::VerifierSourceType source_type,
    const IsCancelledCallback& is_cancelled,
    GetVerifiedContentsCallback verified_contents_callback) {
  DCHECK(source_type ==
         ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES);
  std::unique_ptr<VerifiedContents> verified_contents = ReadVerifiedContents(
      key,
      // If verified_contents.json exists on disk but is invalid, we want to
      // delete it so we can fetch and write the file later if required.
      /*delete_invalid_file=*/true);

  if (verified_contents) {
    std::move(verified_contents_callback)
        .Run(std::move(key), std::move(verified_contents),
             /*did_attempt_fetch=*/false, /*fetch_error=*/net::OK);
    return;
  }

  // Fetch verified_contents.json and then respond.
  FetchVerifiedContents(std::move(key), is_cancelled,
                        std::move(verified_contents_callback));
}

// static
void ContentHash::FetchVerifiedContents(ContentHash::FetchKey key,
                                        const IsCancelledCallback& is_cancelled,
                                        GetVerifiedContentsCallback callback) {
  // |fetcher| deletes itself when it's done.
  internals::ContentHashFetcher* fetcher =
      new internals::ContentHashFetcher(std::move(key));
  fetcher->Start(base::BindOnce(&ContentHash::DidFetchVerifiedContents,
                                std::move(callback)));
}

// static
std::unique_ptr<VerifiedContents> ContentHash::StoreAndRetrieveVerifiedContents(
    std::unique_ptr<std::string> fetched_contents,
    const FetchKey& key) {
  if (!fetched_contents)
    return nullptr;

  // Write file and continue reading hash.
  // Write:
  // Parse the response to make sure it is valid json (on staging sometimes it
  // can be a login redirect html, xml file, etc. if you aren't logged in with
  // the right cookies).  TODO(asargent) - It would be a nice enhancement to
  // move to parsing this in a sandboxed helper (https://crbug.com/372878).
  std::optional<base::Value> parsed = base::JSONReader::Read(*fetched_contents);
  if (!parsed) {
    LOG(ERROR)
        << "Failed to parse fetched verified_contents.json for extension id: "
        << key.extension_id << " version: " << key.extension_version;
    return nullptr;
  }

  VLOG(1) << "JSON parsed ok for " << key.extension_id;
  parsed.reset();  // no longer needed

  base::FilePath destination =
      file_util::GetVerifiedContentsPath(key.extension_root);
  if (!CreateDirAndWriteFile(destination, *fetched_contents)) {
    LOG(ERROR) << "Error writing computed_hashes.json at " << destination;
    return nullptr;
  }

  // Continue reading hash.
  return ReadVerifiedContents(
      key,
      // We've just fetched verified_contents.json, so treat it as a read-only
      // file from now on and do not delete the file even if it turns out to be
      // invalid.
      false /* delete_invalid_file */);
}

// static
void ContentHash::DidFetchVerifiedContents(
    GetVerifiedContentsCallback verified_contents_callback,
    FetchKey key,
    std::unique_ptr<std::string> fetched_contents,
    FetchErrorCode fetch_error) {
  size_t json_size = fetched_contents ? fetched_contents->size() : 0;
  std::unique_ptr<VerifiedContents> verified_contents =
      StoreAndRetrieveVerifiedContents(std::move(fetched_contents), key);

  if (!verified_contents) {
    LOG(ERROR) << "Fetching verified_contents.json for extension id: "
               << key.extension_id << " version: " << key.extension_version
               << " failed with error code " << fetch_error;
    std::move(verified_contents_callback)
        .Run(std::move(key), nullptr, /*did_attempt_fetch=*/true,
             /*fetch_error=*/fetch_error);
    return;
  }

  LOG(WARNING) << "Fetched verified_contents.json with size: " << json_size
               << " bytes for extension id: " << key.extension_id
               << " version: " << key.extension_version;
  RecordFetchResult(true, fetch_error);
  std::move(verified_contents_callback)
      .Run(std::move(key), std::move(verified_contents),
           /*did_attempt_fetch=*/true, /*fetch_error=*/fetch_error);
}

// static
void ContentHash::GetComputedHashes(
    ContentVerifierDelegate::VerifierSourceType source_type,
    const IsCancelledCallback& is_cancelled,
    CreatedCallback created_callback,
    FetchKey key,
    std::unique_ptr<VerifiedContents> verified_contents,
    bool did_attempt_fetch,
    FetchErrorCode fetch_error) {
  if (source_type ==
          ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES &&
      !verified_contents) {
    DCHECK(did_attempt_fetch);
    ContentHash::DispatchFetchFailure(key.extension_id, key.extension_root,
                                      source_type, std::move(created_callback),
                                      is_cancelled, fetch_error);
    return;
  }
  scoped_refptr<ContentHash> hash =
      new ContentHash(key.extension_id, key.extension_root, source_type,
                      std::move(verified_contents));
  hash->BuildComputedHashes(did_attempt_fetch, /*force_build=*/false,
                            is_cancelled);
  std::move(created_callback).Run(hash, is_cancelled && is_cancelled.Run());
}

// static
void ContentHash::DispatchFetchFailure(
    const ExtensionId& extension_id,
    const base::FilePath& extension_root,
    ContentVerifierDelegate::VerifierSourceType source_type,
    CreatedCallback created_callback,
    const IsCancelledCallback& is_cancelled,
    FetchErrorCode fetch_error) {
  DCHECK_EQ(ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES,
            source_type)
      << "Only signed hashes should attempt fetching verified_contents.json";
  RecordFetchResult(false, fetch_error);
  // NOTE: bare new because ContentHash constructor is private.
  scoped_refptr<ContentHash> content_hash =
      new ContentHash(extension_id, extension_root, source_type, nullptr);
  std::move(created_callback)
      .Run(content_hash, is_cancelled && is_cancelled.Run());
}

// static
void ContentHash::RecordFetchResult(bool success, int fetch_error) {
  UMA_HISTOGRAM_BOOLEAN("Extensions.ContentVerification.FetchResult", success);
  if (!success) {
    base::UmaHistogramSparse("Extensions.ContentVerification.FetchFailureError",
                             fetch_error);
  }
}

bool ContentHash::ShouldComputeHashesForResource(
    const base::FilePath& relative_unix_path) {
  if (source_type_ !=
      ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES) {
    return true;
  }
  DCHECK(verified_contents_);
  return verified_contents_->HasTreeHashRoot(relative_unix_path);
}

std::set<base::FilePath> ContentHash::GetMismatchedComputedHashes(
    ComputedHashes::Data* computed_hashes_data) {
  DCHECK(computed_hashes_data);
  if (source_type_ !=
      ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES) {
    return {};
  }

  std::set<base::FilePath> mismatched_hashes;

  for (const auto& resource_info : computed_hashes_data->items()) {
    const ComputedHashes::Data::HashInfo& hash_info = resource_info.second;
    const content_verifier_utils::CanonicalRelativePath&
        canonical_relative_path = resource_info.first;

    std::string root = ComputeTreeHashRoot(hash_info.hashes,
                                           block_size_ / crypto::kSHA256Length);
    if (!verified_contents_->TreeHashRootEqualsForCanonicalPath(
            canonical_relative_path, root)) {
      mismatched_hashes.insert(hash_info.relative_unix_path);
    }
  }

  return mismatched_hashes;
}

bool ContentHash::CreateHashes(const base::FilePath& hashes_file,
                               const IsCancelledCallback& is_cancelled) {
  DCHECK_EQ(ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES,
            source_type_);
  base::ElapsedTimer timer;
  did_attempt_creating_computed_hashes_ = true;

  std::optional<ComputedHashes::Data> computed_hashes_data =
      ComputedHashes::Compute(
          extension_root_, block_size_, is_cancelled,
          // Using base::Unretained is safe here as
          // ShouldComputeHashesForResource is only called synchronously from
          // ComputedHashes::Compute.
          base::BindRepeating(&ContentHash::ShouldComputeHashesForResource,
                              base::Unretained(this)));

  if (computed_hashes_data) {
    std::set<base::FilePath> hashes_mismatch =
        GetMismatchedComputedHashes(&computed_hashes_data.value());
    for (const auto& relative_unix_path : hashes_mismatch) {
      VLOG(1) << "content mismatch for " << relative_unix_path.AsUTF8Unsafe();
      // Remove hash entry to keep computed_hashes.json file clear of mismatched
      // hashes.
      computed_hashes_data->Remove(relative_unix_path);
    }
    hash_mismatch_unix_paths_ = std::move(hashes_mismatch);
  }

  bool result = computed_hashes_data &&
                ComputedHashes(std::move(computed_hashes_data.value()))
                    .WriteToFile(hashes_file);
  UMA_HISTOGRAM_TIMES("ExtensionContentHashFetcher.CreateHashesTime",
                      timer.Elapsed());

  return result;
}

void ContentHash::BuildComputedHashes(bool attempted_fetching_verified_contents,
                                      bool force_build,
                                      const IsCancelledCallback& is_cancelled) {
  base::FilePath computed_hashes_path =
      file_util::GetComputedHashesPath(extension_root_);

  // Create computed_hashes.json file if any of the following is true:
  // - We just fetched and wrote a verified_contents.json (i.e.
  //   |attempted_fetching_verified_contents| = true).
  // - computed_hashes.json is missing.
  // - existing computed_hashes.json cannot be initialized correctly (probably
  //   due to disk corruption).
  //  bool will_create = attempted_fetching_verified_contents ||
  //                     !base::PathExists(computed_hashes_path);

  // However, existing behavior is to create computed_hashes.json only when
  // ContentVerifyJob's request to computed_hashes.json fails.
  // TODO(lazyboy): Fix this and use |will_create| condition from the comment
  // above, see https://crbug.com/819832 for details.
  bool will_create =
      (force_build || !base::PathExists(computed_hashes_path)) &&
      // Note that we are not allowed to create computed_hashes.json file
      // without hashes signature (verified_contents.json) because files could
      // already be corrupted.
      source_type_ ==
          ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES;
  if (!will_create) {
    // Note: Tolerate for existing implementation.
    // Try to read and initialize the file first. On failure, continue creating.
    std::optional<ComputedHashes> computed_hashes =
        ComputedHashes::CreateFromFile(computed_hashes_path,
                                       &computed_hashes_status_);
    DCHECK_EQ(computed_hashes_status_ == ComputedHashes::Status::SUCCESS,
              computed_hashes.has_value());
    if (!computed_hashes) {
      // TODO(lazyboy): Also create computed_hashes.json in this case. See the
      // comment above about |will_create|.
      // will_create = true;
    } else {
      // Read successful.
      computed_hashes_ =
          std::make_unique<ComputedHashes>(std::move(computed_hashes.value()));
      return;
    }
  }

  if (will_create && !CreateHashes(computed_hashes_path, is_cancelled)) {
    // Failed creating computed_hashes.json.
    return;
  }

  // Read computed_hashes.json:
  if (!base::PathExists(computed_hashes_path))
    return;

  std::optional<ComputedHashes> computed_hashes =
      ComputedHashes::CreateFromFile(computed_hashes_path,
                                     &computed_hashes_status_);
  DCHECK_EQ(computed_hashes_status_ == ComputedHashes::Status::SUCCESS,
            computed_hashes.has_value());
  if (!computed_hashes)
    return;

  // Read successful.
  computed_hashes_ =
      std::make_unique<ComputedHashes>(std::move(computed_hashes.value()));
}

}  // namespace extensions
