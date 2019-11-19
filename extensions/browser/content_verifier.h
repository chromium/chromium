// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observer.h"
#include "base/version.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/content_verifier_io_data.h"
#include "extensions/browser/content_verify_job.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
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
class ContentVerifier : public base::RefCountedThreadSafe<ContentVerifier>,
                        public ExtensionRegistryObserver {
 public:
  class TestObserver {
   public:
    virtual void OnFetchComplete(const std::string& extension_id,
                                 bool success) = 0;
  };

  static void SetObserverForTests(TestObserver* observer);

  ContentVerifier(content::BrowserContext* context,
                  std::unique_ptr<ContentVerifierDelegate> delegate);
  void Start();
  void Shutdown();

  // Call this before reading a file within an extension. Returns and starts a
  // content verify job if the specified resource requires content verification,
  // otherwise returns nullptr.
  scoped_refptr<ContentVerifyJob> CreateAndStartJobFor(
      const std::string& extension_id,
      const base::FilePath& extension_root,
      const base::FilePath& relative_path);

  // ExtensionRegistryObserver interface
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  using ContentHashCallback =
      base::OnceCallback<void(scoped_refptr<const ContentHash>)>;

  // Retrieves ContentHash for an extension through |callback|.
  // Must be called on IO thread.
  // |callback| is called on IO thread.
  // |force_missing_computed_hashes_creation| should be true if
  // computed_hashes.json is required to be created if that file is missing or
  // unreadable.
  // TODO(lazyboy): |force_missing_computed_hashes_creation| should always be
  // true, handing its behavior adds extra complexity in HashHelper and this
  // param should be removed when we can unify/fix computed_hashes.json
  // treatment, see https://crbug.com/819832 for details.
  void GetContentHash(const ExtensionId& extension_id,
                      const base::FilePath& extension_root,
                      const base::Version& extension_version,
                      bool force_missing_computed_hashes_creation,
                      ContentHashCallback callback);

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

 private:
  friend class ContentVerifierTest;
  friend class base::RefCountedThreadSafe<ContentVerifier>;
  friend class HashHelper;
  ~ContentVerifier() override;

  void ShutdownOnIO();

  struct CacheKey;
  class HashHelper;

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

  // Returns true if any of the paths in |relative_unix_paths| *should* have
  // their contents verified. (Some files get transcoded during the install
  // process, so we don't want to verify their contents because they are
  // expected not to match).
  bool ShouldVerifyAnyPaths(
      const std::string& extension_id,
      const base::FilePath& extension_root,
      const std::set<base::FilePath>& relative_unix_paths);

  // Called (typically by a verification job) to indicate that verification
  // failed while reading some file in |extension_id|.
  void VerifyFailed(const ExtensionId& extension_id,
                    ContentVerifyJob::FailureReason reason);

  // Returns the HashHelper instance, making sure we create it at most once.
  // Must *not* be called after |shutdown_on_io_| is set to true.
  HashHelper* GetOrCreateHashHelper();

  // Set to true once we've begun shutting down on UI thread.
  // Updated and accessed only on UI thread.
  bool shutdown_on_ui_ = false;

  // Set to true once we've begun shutting down on IO thread.
  // Updated and accessed only on IO thread.
  bool shutdown_on_io_ = false;

  content::BrowserContext* const context_;

  // Guards creation of |hash_helper_|, limiting number of creation to <= 1.
  // Accessed only on IO thread.
  bool hash_helper_created_ = false;

  // Created and used on IO thread.
  std::unique_ptr<HashHelper, content::BrowserThread::DeleteOnIOThread>
      hash_helper_;

  std::map<CacheKey, scoped_refptr<const ContentHash>> cache_;

  std::unique_ptr<ContentVerifierDelegate> delegate_;

  // For observing the ExtensionRegistry.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer_{this};

  // Data that should only be used on the IO thread.
  ContentVerifierIOData io_data_;

  DISALLOW_COPY_AND_ASSIGN(ContentVerifier);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_
