// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_hash.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_hash_fetcher.h"
#include "extensions/browser/content_hash_tree.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/file_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace extensions {

namespace {

using SortedFilePathSet = std::set<base::FilePath>;

bool CreateDirAndWriteFile(const base::FilePath& destination,
                           const std::string& content) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  base::FilePath dir = destination.DirName();
  if (!base::CreateDirectory(dir))
    return false;

  int write_result =
      base::WriteFile(destination, content.data(), content.size());
  return write_result >= 0 &&
         base::checked_cast<size_t>(write_result) == content.size();
}

std::unique_ptr<VerifiedContents> ReadVerifiedContents(
    const ContentHash::FetchKey& key,
    bool delete_invalid_file) {
  DCHECK(GetExtensionFileTaskRunner()->RunsTasksInCurrentSequence());
  base::FilePath verified_contents_path =
      file_util::GetVerifiedContentsPath(key.extension_root);
  std::unique_ptr<VerifiedContents> verified_contents =
      VerifiedContents::Create(key.verifier_key, verified_contents_path);
  if (!verified_contents) {
    if (delete_invalid_file &&
        !base::DeleteFile(verified_contents_path, false)) {
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
                      /*did_attempt_fetch=*/false);
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

const ComputedHashes::Reader& ContentHash::computed_hashes() const {
  DCHECK(succeeded_ && computed_hashes_);
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
    std::unique_ptr<VerifiedContents> verified_contents,
    std::unique_ptr<ComputedHashes::Reader> computed_hashes)
    : extension_id_(id),
      extension_root_(root),
      source_type_(source_type),
      verified_contents_(std::move(verified_contents)),
      computed_hashes_(std::move(computed_hashes)) {
  succeeded_ = verified_contents_ != nullptr && computed_hashes_ != nullptr;
}

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
             /*did_attempt_fetch=*/false);
    return;
  }

  // Fetch verified_contents.json and then respond.
  FetchVerifiedContents(std::move(key), is_cancelled,
                        std::move(verified_contents_callback));
}

// static
void ContentHash::FetchVerifiedContents(
    ContentHash::FetchKey key,
    const ContentHash::IsCancelledCallback& is_cancelled,
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
  base::Optional<base::Value> parsed =
      base::JSONReader::Read(*fetched_contents);
  if (!parsed)
    return nullptr;

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
    std::unique_ptr<std::string> fetched_contents) {
  std::unique_ptr<VerifiedContents> verified_contents =
      StoreAndRetrieveVerifiedContents(std::move(fetched_contents), key);

  if (!verified_contents) {
    std::move(verified_contents_callback)
        .Run(std::move(key), nullptr, /*did_attempt_fetch=*/true);
    return;
  }

  RecordFetchResult(true);
  std::move(verified_contents_callback)
      .Run(std::move(key), std::move(verified_contents),
           /*did_attempt_fetch=*/true);
}

// static
void ContentHash::GetComputedHashes(
    ContentVerifierDelegate::VerifierSourceType source_type,
    const IsCancelledCallback& is_cancelled,
    CreatedCallback created_callback,
    FetchKey key,
    std::unique_ptr<VerifiedContents> verified_contents,
    bool did_attempt_fetch) {
  if (source_type ==
          ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES &&
      !verified_contents) {
    DCHECK(did_attempt_fetch);
    ContentHash::DispatchFetchFailure(key.extension_id, key.extension_root,
                                      source_type, std::move(created_callback),
                                      is_cancelled);
    return;
  }
  scoped_refptr<ContentHash> hash =
      new ContentHash(key.extension_id, key.extension_root, source_type,
                      std::move(verified_contents), nullptr);
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
    const IsCancelledCallback& is_cancelled) {
  DCHECK_EQ(ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES,
            source_type)
      << "Only signed hashes should attempt fetching verified_contents.json";
  RecordFetchResult(false);
  // NOTE: bare new because ContentHash constructor is private.
  scoped_refptr<ContentHash> content_hash = new ContentHash(
      extension_id, extension_root, source_type, nullptr, nullptr);
  std::move(created_callback)
      .Run(content_hash, is_cancelled && is_cancelled.Run());
}

// static
void ContentHash::RecordFetchResult(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Extensions.ContentVerification.FetchResult", success);
}

base::Optional<std::vector<std::string>>
ContentHash::ComputeAndCheckResourceHash(
    const base::FilePath& full_path,
    const base::FilePath& relative_unix_path) {
  DCHECK(source_type_ !=
             ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES ||
         verified_contents_);

  if (source_type_ ==
          ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES &&
      !verified_contents_->HasTreeHashRoot(relative_unix_path)) {
    return base::nullopt;
  }

  std::string contents;
  if (!base::ReadFileToString(full_path, &contents)) {
    LOG(ERROR) << "Could not read " << full_path.MaybeAsASCII();
    return base::nullopt;
  }

  // Iterate through taking the hash of each block of size (block_size_) of
  // the file.
  std::vector<std::string> hashes =
      ComputedHashes::GetHashesForContent(contents, block_size_);
  if (source_type_ !=
      ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES) {
    return base::make_optional(std::move(hashes));
  }

  std::string root =
      ComputeTreeHashRoot(hashes, block_size_ / crypto::kSHA256Length);
  if (!verified_contents_->TreeHashRootEquals(relative_unix_path, root)) {
    VLOG(1) << "content mismatch for " << relative_unix_path.AsUTF8Unsafe();
    hash_mismatch_unix_paths_.insert(relative_unix_path);
    return base::nullopt;
  }

  return base::make_optional(std::move(hashes));
}

bool ContentHash::CreateHashes(const base::FilePath& hashes_file,
                               const IsCancelledCallback& is_cancelled) {
  base::ElapsedTimer timer;
  did_attempt_creating_computed_hashes_ = true;
  // Make sure the directory exists.
  if (!base::CreateDirectoryAndGetError(hashes_file.DirName(), nullptr))
    return false;

  base::FileEnumerator enumerator(extension_root_, true, /* recursive */
                                  base::FileEnumerator::FILES);
  // First discover all the file paths and put them in a sorted set.
  SortedFilePathSet paths;
  for (;;) {
    if (is_cancelled && is_cancelled.Run())
      return false;

    base::FilePath full_path = enumerator.Next();
    if (full_path.empty())
      break;
    paths.insert(full_path);
  }

  // Now iterate over all the paths in sorted order and compute the block hashes
  // for each one.
  ComputedHashes::Writer writer;
  for (auto i = paths.begin(); i != paths.end(); ++i) {
    if (is_cancelled && is_cancelled.Run())
      return false;

    const base::FilePath& full_path = *i;
    base::FilePath relative_unix_path;
    extension_root_.AppendRelativePath(full_path, &relative_unix_path);
    relative_unix_path = relative_unix_path.NormalizePathSeparatorsTo('/');

    base::Optional<std::vector<std::string>> hashes =
        ComputeAndCheckResourceHash(full_path, relative_unix_path);
    if (hashes)
      writer.AddHashes(relative_unix_path, block_size_, *hashes);
  }
  bool result = writer.WriteToFile(hashes_file);
  UMA_HISTOGRAM_TIMES("ExtensionContentHashFetcher.CreateHashesTime",
                      timer.Elapsed());

  if (result)
    succeeded_ = true;

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
    auto computed_hashes = std::make_unique<ComputedHashes::Reader>();
    if (!computed_hashes->InitFromFile(computed_hashes_path)) {
      // TODO(lazyboy): Also create computed_hashes.json in this case. See the
      // comment above about |will_create|.
      // will_create = true;
    } else {
      // Read successful.
      succeeded_ = true;
      computed_hashes_ = std::move(computed_hashes);
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

  auto computed_hashes = std::make_unique<ComputedHashes::Reader>();
  if (!computed_hashes->InitFromFile(computed_hashes_path))
    return;

  // Read successful.
  succeeded_ = true;
  computed_hashes_ = std::move(computed_hashes);
}

}  // namespace extensions
