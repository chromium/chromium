// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_HASH_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_HASH_H_

#include <set>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/version.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/content_verifier/content_verifier_delegate.h"
#include "extensions/browser/content_verifier/content_verifier_key.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace extensions {

// Represents content verification hashes for an extension.
//
// Instances can be created using Create() factory method on sequences with
// blocking IO access. If hash retrieval succeeds then ContentHash::succeeded()
// will return true and
// a. ContentHash::verified_contents() will return structured representation of
//    verified_contents.json
// b. ContentHash::computed_hashes() will return structured representation of
//    computed_hashes.json.
//
// If verified_contents.json was missing on disk (e.g. because of disk
// corruption or such), this class will fetch the file from network. After
// fetching the class will parse/validate this data as needed, including
// calculating expected hashes for each block of each file within an extension.
// (These unsigned leaf node block level hashes will always be checked at time
// of use use to make sure they match the signed treehash root hash).
//
// computed_hashes.json is computed over the files in an extension's directory.
// If computed_hashes.json was required to be written to disk and
// it was successful, ContentHash::hash_mismatch_unix_paths() will return all
// FilePaths from the extension directory that had content verification
// mismatch.
//
// Clients of this class can cancel the disk write operation of
// computed_hashes.json while it is ongoing. This is because it can potentially
// take long time. This cancellation can be performed through |is_cancelled|.
class ContentHash : public base::RefCountedThreadSafe<ContentHash> {
 public:
  // The combined (network or http response) error code while fetching.
  using FetchErrorCode = int;

  // Holds key to identify an extension for content verification, parameters to
  // fetch verified_contents.json and other supplementary info.
  struct FetchKey {
    // Extension info.
    ExtensionId extension_id;
    base::FilePath extension_root;
    base::Version extension_version;

    // Fetch parameters.
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        url_loader_factory_remote;
    GURL fetch_url;

    // The key used to validate verified_contents.json.
    ContentVerifierKey verifier_key;

    FetchKey(const ExtensionId& extension_id,
             const base::FilePath& extension_root,
             const base::Version& extension_version,
             mojo::PendingRemote<network::mojom::URLLoaderFactory>
                 url_loader_factory_remote,
             const GURL& fetch_url,
             ContentVerifierKey verifier_key);

    FetchKey(const FetchKey&) = delete;
    FetchKey& operator=(const FetchKey&) = delete;

    ~FetchKey();

    FetchKey(FetchKey&& other);
    FetchKey& operator=(FetchKey&& other);
  };

  // Result of checking tree hash root (typically calculated from block hashes
  // in computed_hashes.json) against signed hash from verified_contents.json.
  enum class TreeHashVerificationResult {
    // Hash is correct.
    SUCCESS,

    // There is no such file in verified_contents.json.
    NO_ENTRY,

    // Hash does not match the one from verified_contents.json.
    HASH_MISMATCH
  };

  ContentHash(const ContentHash&) = delete;
  ContentHash& operator=(const ContentHash&) = delete;

  // Factory:
  // Returns ContentHash through |created_callback|, the returned values are:
  //   - |hash| The content hash. This will never be nullptr, but
  //     verified_contents or computed_hashes may be empty if something fails.
  //   - |was_cancelled| Indicates whether or not the request was cancelled
  //     through |is_cancelled|, while it was being processed.
  using CreatedCallback =
      base::OnceCallback<void(scoped_refptr<ContentHash> hash,
                              bool was_cancelled)>;
  static void Create(FetchKey key,
                     ContentVerifierDelegate::VerifierSourceType source_type,
                     const IsCancelledCallback& is_cancelled,
                     CreatedCallback created_callback);

  // Forces creation of computed_hashes.json. Must be called with after
  // |verified_contents| has been successfully set.
  // TODO(lazyboy): Remove this once https://crbug.com/819832 is fixed.
  void ForceBuildComputedHashes(const IsCancelledCallback& is_cancelled,
                                CreatedCallback created_callback);

  // Returns the result of comparing tree hash |root| for the |relative_path| to
  // verified_contens.json data.
  TreeHashVerificationResult VerifyTreeHashRoot(
      const base::FilePath& relative_path,
      const std::string* root) const;

  const ComputedHashes& computed_hashes() const;

  // Returns loading status of hashes.
  ComputedHashes::Status computed_hashes_status() const {
    return computed_hashes_status_;
  }

  // Returns whether or not computed_hashes.json (and, if needed,
  // verified_contents.json too) was read correctly and is ready to use.
  bool succeeded() const {
    return computed_hashes_status_ == ComputedHashes::Status::SUCCESS;
  }

  // If ContentHash creation writes computed_hashes.json, then this returns the
  // FilePaths whose content hash didn't match expected hashes.
  const std::set<base::FilePath>& hash_mismatch_unix_paths() const {
    return hash_mismatch_unix_paths_;
  }
  const ExtensionId& extension_id() const { return extension_id_; }
  const base::FilePath& extension_root() const { return extension_root_; }

  // Returns whether or not computed_hashes.json re-creation might be required
  // for |this| to succeed.
  // TODO(lazyboy): Remove this once https://crbug.com/819832 is fixed.
  bool might_require_computed_hashes_force_creation() const {
    return !succeeded() && verified_contents_ != nullptr &&
           !did_attempt_creating_computed_hashes_;
  }

  static std::string ComputeTreeHashForContent(const std::string& contents,
                                               int block_size);

 private:
  friend class base::RefCountedThreadSafe<ContentHash>;

  using GetVerifiedContentsCallback = base::OnceCallback<void(
      FetchKey key,
      std::unique_ptr<VerifiedContents> verified_contents,
      bool did_attempt_fetch,
      FetchErrorCode fetch_error)>;

  ContentHash(const ExtensionId& id,
              const base::FilePath& root,
              ContentVerifierDelegate::VerifierSourceType source_type,
              std::unique_ptr<const VerifiedContents> verified_contents);
  ~ContentHash();

  // Step 1/2: verified_contents.json.
  static void GetVerifiedContents(
      FetchKey key,
      ContentVerifierDelegate::VerifierSourceType source_type,
      const IsCancelledCallback& is_cancelled,
      GetVerifiedContentsCallback);
  static void FetchVerifiedContents(FetchKey key,
                                    const IsCancelledCallback& is_cancelled,
                                    GetVerifiedContentsCallback callback);
  static std::unique_ptr<VerifiedContents> StoreAndRetrieveVerifiedContents(
      std::unique_ptr<std::string> fetched_contents,
      const FetchKey& key);
  static void DidFetchVerifiedContents(
      GetVerifiedContentsCallback callback,
      FetchKey key,
      std::unique_ptr<std::string> fetched_contents,
      FetchErrorCode fetch_error);

  // Step 2/2: computed_hashes.json.
  static void GetComputedHashes(
      ContentVerifierDelegate::VerifierSourceType source_type,
      const IsCancelledCallback& is_cancelled,
      CreatedCallback created_callback,
      FetchKey key,
      std::unique_ptr<VerifiedContents> verified_contents,
      bool did_attempt_fetch,
      FetchErrorCode fetch_error);

  static void DispatchFetchFailure(
      const ExtensionId& extension_id,
      const base::FilePath& extension_root,
      ContentVerifierDelegate::VerifierSourceType source_type,
      CreatedCallback created_callback,
      const IsCancelledCallback& is_cancelled,
      FetchErrorCode fetch_error);

  static void RecordFetchResult(bool success, FetchErrorCode fetch_error);

  // Computes hashes for all files in |key_.extension_root|, and uses
  // a ComputedHashes::Writer to write that information into |hashes_file|.
  // Returns true on success.
  // The verified contents file from the webstore only contains the treehash
  // root hash, but for performance we want to cache the individual block level
  // hashes. This function will create that cache with block-level hashes for
  // each file in the extension if needed (the treehash root hash for each of
  // these should equal what is in the verified contents file from the
  // webstore).
  bool CreateHashes(const base::FilePath& hashes_file,
                    const IsCancelledCallback& is_cancelled);

  // Builds computed_hashes. Possibly after creating computed_hashes.json file
  // if necessary.
  void BuildComputedHashes(bool did_fetch_verified_contents,
                           bool force_build,
                           const IsCancelledCallback& is_cancelled);

  // Helper callback for ComputedHashes::Create. Checks whether we want the hash
  // of the given resource be in computes_hashes.json or not.
  bool ShouldComputeHashesForResource(const base::FilePath& relative_unix_path);

  // If needed (|source_type_| is SIGNED_HASHES) checks each hash from
  // |computed_hashes| against data from verified_contenst.json and returns list
  // of mismatches. If not needed, just returns empty list.
  std::set<base::FilePath> GetMismatchedComputedHashes(
      ComputedHashes::Data* computed_hashes);

  bool has_verified_contents() const { return verified_contents_ != nullptr; }

  const ExtensionId extension_id_;
  const base::FilePath extension_root_;
  ContentVerifierDelegate::VerifierSourceType source_type_;

  ComputedHashes::Status computed_hashes_status_ =
      ComputedHashes::Status::UNKNOWN;

  bool did_attempt_creating_computed_hashes_ = false;

  // TODO(lazyboy): Avoid dynamic allocations here, |this| already supports
  // move.
  std::unique_ptr<const VerifiedContents> verified_contents_;
  std::unique_ptr<const ComputedHashes> computed_hashes_;

  // Paths that were found to have a mismatching hash.
  std::set<base::FilePath> hash_mismatch_unix_paths_;

  // The block size to use for hashing.
  // TODO(asargent) - use the value from verified_contents.json for each
  // file, instead of using a constant.
  int block_size_ = extension_misc::kContentVerificationDefaultBlockSize;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_HASH_H_
