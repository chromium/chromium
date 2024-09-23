// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_H_

#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/version.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/content_verifier/content_verifier_delegate.h"
#include "extensions/browser/content_verifier/content_verifier_io_data.h"
#include "extensions/browser/content_verifier/content_verify_job.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;

// Used for managing overall content verification - both fetching content
// hashes as needed, and supplying job objects to verify file contents as they
// are read.
//
// Some notes about extension resource paths:
// An extension resource path is a path relative to it's extension root
// directory. For the purposes of content verification system, there can be
// several transformations of the relative path:
//   1. Relative path: Relative path as is. This is base::FilePath that simply
//      is the relative path of the resource.
//   2. Relative unix path: Some underlying parts of content-verification
//      require uniform separator, we use '/' as separator so it is effectively
//      unix style. Note that this is a reversible transformation.
//   3. content_verifier_utils::CanonicalRelativePath:
//      Canonicalized relative paths are used as keys of maps within
//      VerifiedContents and ComputedHashes. This takes care of OS specific file
//      access issues:
//      - windows/mac is case insensitive while accessing files.
//      - windows ignores (.| )+ suffixes in filename while accessing a file.
//      Canonicalization consists of normalizing the separators, lower casing
//      the filepath in case-insensitive systems and trimming ignored suffixes
//      if appropriate.
//      See content_verifier_utils::CanonicalizeRelativePath() for details.
class ContentVerifier : public base::RefCountedThreadSafe<ContentVerifier>,
                        public ExtensionRegistryObserver {
 public:
  class TestObserver {
   public:
    virtual void OnFetchComplete(
        const scoped_refptr<const ContentHash>& content_hash,
        bool did_hash_mismatch) = 0;
  };

  static void SetObserverForTests(TestObserver* observer);

  ContentVerifier(content::BrowserContext* context,
                  std::unique_ptr<ContentVerifierDelegate> delegate);

  ContentVerifier(const ContentVerifier&) = delete;
  ContentVerifier& operator=(const ContentVerifier&) = delete;

  void Start();
  void Shutdown();

  // Call this before reading a file within an extension. Returns a verify job
  // to be used for content verification.
  static scoped_refptr<ContentVerifyJob> CreateAndStartJobFor(
      const ExtensionId& extension_id,
      const base::FilePath& extension_root,
      const base::FilePath& relative_path,
      scoped_refptr<ContentVerifier> verifier);

  // ExtensionRegistryObserver interface
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  using ContentHashCallback =
      base::OnceCallback<void(scoped_refptr<const ContentHash>)>;

  // Creates, adds to cache, and returns ContentHash for an extension through
  // |callback|.
  // Must be called on IO thread.
  // |callback| is called on IO thread.
  // |force_missing_computed_hashes_creation| should be true if
  // computed_hashes.json is required to be created if that file is missing or
  // unreadable.
  // TODO(lazyboy): |force_missing_computed_hashes_creation| should always be
  // true, handing its behavior adds extra complexity in HashHelper and this
  // param should be removed when we can unify/fix computed_hashes.json
  // treatment, see https://crbug.com/819832 for details.
  void CreateContentHash(const ExtensionId& extension_id,
                         const base::FilePath& extension_root,
                         const base::Version& extension_version,
                         bool force_missing_computed_hashes_creation,
                         ContentHashCallback callback);

  // Retrieves cached ContentHash for an extension synchronously. If the hash is
  // not found in the cache, returns nullptr.
  scoped_refptr<const ContentHash> GetCachedContentHash(
      const ExtensionId& extension_id,
      const base::Version& extension_version,
      bool force_missing_computed_hashes_creation);

  // Returns whether or not we should compute hashes during installation.
  // Typically we don't need this when extension has verified (signed) resources
  // hashes, as we can postpone hashes computing to the time we'll need them and
  // check there. But without signed hashes we may not compute hashes at
  // arbitrary time, we are only allowed to do it during installation.
  bool ShouldComputeHashesOnInstall(const Extension& extension);

  // Returns public key used to check content verification data. Normally it's
  // Chrome Web Store key, but may be overridden in tests via delegate.
  ContentVerifierKey GetContentVerifierKey();

  GURL GetSignatureFetchUrlForTest(const ExtensionId& extension_id,
                                   const base::Version& extension_version);

  // Exposes VerifyFailed for tests.
  void VerifyFailedForTest(const ExtensionId& extension_id,
                           ContentVerifyJob::FailureReason reason);

  // Test helper to recompute |io_data_| for |extension| without having to
  // call |OnExtensionLoaded|.
  void ResetIODataForTesting(const Extension* extension);

  // Test helper to clear all cached ContentHash entries from |cache_|.
  void ClearCacheForTesting();

  // Test helper to normalize relative path of file.
  static base::FilePath NormalizeRelativePathForTesting(
      const base::FilePath& path);

  bool ShouldVerifyAnyPathsForTesting(
      const ExtensionId& extension_id,
      const base::FilePath& extension_root,
      const std::set<base::FilePath>& relative_unix_paths);

  void OverrideDelegateForTesting(
      std::unique_ptr<ContentVerifierDelegate> delegate);

 private:
  friend class base::RefCountedThreadSafe<ContentVerifier>;
  friend class HashHelper;
  ~ContentVerifier() override;

  enum class VerifiedFileType {
    kNone,                 // Not a file to be verified.
    kBackgroundPage,       // The background page.
    kBackgroundScript,     // A script in a generated background page.
    kServiceWorkerScript,  // The extension service worker script.
    kContentScript,        // A JS script in a content script.
    kMiscHtmlFile,  // A general HTML file (e.g. that might be loaded in a tab).
    kMiscJsFile,    // A general JS file (e.g. that might be loaded in a tab).
    kMiscFile,      // Any other file that should be verified.

    kMaxValue = kMiscFile,
  };

  void StartOnIO();
  void ShutdownOnIO();

  // Called after a verification job is created.
  void OnJobCreated(scoped_refptr<ContentVerifyJob> job);

  // If a verification is needed, starts the verification job and returns true.
  // Otherwise, returns false without starting the job.
  bool StartJob(const scoped_refptr<ContentVerifyJob>& job);

  struct CacheKey;
  class HashHelper;
  class VerifiedFileTypeHelper;

  void OnFetchComplete(const scoped_refptr<const ContentHash>& content_hash);
  ContentHash::FetchKey GetFetchKey(const ExtensionId& extension_id,
                                    const base::FilePath& extension_root,
                                    const base::Version& extension_version);

  void DidGetContentHash(const CacheKey& cache_key,
                         ContentHashCallback orig_callback,
                         scoped_refptr<const ContentHash> content_hash);

  // Binds an URLLoaderFactoryReceiver on the UI thread.
  void BindURLLoaderFactoryReceiverOnUIThread(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          url_loader_factory_receiver);

  // Performs IO thread operations after extension load.
  void OnExtensionLoadedOnIO(
      const ExtensionId& extension_id,
      const base::FilePath& extension_root,
      const base::Version& extension_version,
      std::unique_ptr<ContentVerifierIOData::ExtensionData> data);
  // Performs IO thread operations after extension unload.
  void OnExtensionUnloadedOnIO(const ExtensionId& extension_id,
                               const base::Version& extension_version);

  // Called to indicate that all the relevant data is ready for the extension,
  // and we can start verifying files.
  void OnExtensionDataReady(const ExtensionId& extension_id);

  // Called (typically by a verification job) to indicate that verification
  // failed while reading some file in `extension_id`. `failed_file_types` and
  // `manifest_version` indicate additional data about which file was detected
  // as corrupted.
  void VerifyFailed(const ExtensionId& extension_id,
                    const std::vector<VerifiedFileType>& failed_file_types,
                    int manifest_version,
                    ContentVerifyJob::FailureReason reason);

  // Returns the HashHelper instance, making sure we create it at most once.
  // Must *not* be called after |shutdown_on_io_| is set to true.
  HashHelper* GetOrCreateHashHelper();

  // Set to true once |Start| is called to enable content verification. Updated
  // and accessed only on IO thread.
  bool verification_enabled_ = false;

  // A set of extension ids for which the verification data is ready. Updated
  // and accessed only on IO thread.
  std::unordered_set<ExtensionId> ready_extensions_;

  // Jobs which are waiting for the extension data to be ready. Updated and
  // accessed only on IO thread.
  std::unordered_map<ExtensionId, std::vector<scoped_refptr<ContentVerifyJob>>>
      pending_jobs_;

  // Set to true once we've begun shutting down on UI thread.
  // Updated and accessed only on UI thread.
  bool shutdown_on_ui_ = false;

  // Set to true once we've begun shutting down on IO thread.
  // Updated and accessed only on IO thread.
  bool shutdown_on_io_ = false;

  const raw_ptr<content::BrowserContext, AcrossTasksDanglingUntriaged> context_;

  // Guards creation of |hash_helper_|, limiting number of creation to <= 1.
  // Accessed only on IO thread.
  bool hash_helper_created_ = false;

  // Created and used on IO thread.
  std::unique_ptr<HashHelper, content::BrowserThread::DeleteOnIOThread>
      hash_helper_;

  std::map<CacheKey, scoped_refptr<const ContentHash>> cache_;

  std::unique_ptr<ContentVerifierDelegate> delegate_;

  // For observing the ExtensionRegistry.
  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      observation_{this};

  // Data that should only be used on the IO thread.
  ContentVerifierIOData io_data_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFIER_H_
