// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_REGISTRY_H_
#define CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_REGISTRY_H_

#include <stdint.h>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_staging_writer.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_types.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

class BrowserContext;

// Result of a read request.
struct CONTENT_EXPORT CrossOriginStorageOpenResult {
  CrossOriginStorageOpenResult();
  CrossOriginStorageOpenResult(CrossOriginStorageOpenResult&&);
  CrossOriginStorageOpenResult& operator=(CrossOriginStorageOpenResult&&);
  CrossOriginStorageOpenResult(const CrossOriginStorageOpenResult&) = delete;
  CrossOriginStorageOpenResult& operator=(const CrossOriginStorageOpenResult&) =
      delete;
  ~CrossOriginStorageOpenResult();

  CrossOriginStorageStatus status = CrossOriginStorageStatus::kOperationFailed;

  // Path of the committed resource. The browser registers a blob against this
  // path so the renderer can be handed a readable File; the renderer never
  // learns the path itself.
  base::FilePath path;

  int64_t size = 0;
};

// Owns the Cross-Origin Storage cache for one profile.
//
// The registry is attached to BrowserContext rather than StoragePartition on
// purpose: the point of Cross-Origin Storage is to de-duplicate byte-identical
// resources across origins, so the cache must not be partitioned by
// StorageKey. Off-the-record (Incognito) profiles currently return nullptr from
// GetOrCreateForBrowserContext() so they never share the parent profile's
// directory, leak browsing state, or act as an Incognito mode oracle.
//
// Public methods are called on the UI thread; all disk access and all metadata
// live on a blocking sequence owned by `backend_`.
//
// On-disk layout, rooted at <profile>/CrossOriginStorage:
//   metadata.json          entry metadata (storing origins, sharing scope,
//                          last access time)
//   blobs/<algorithm>/<hex> committed, content-addressed resources
//   staging/<uuid>.tmp     quarantined in-flight writes, never readable
//
// TODO(crbug.com/547302825): Integrate with BrowsingDataRemover to remove
// cleared origins from `storing_origins` and delete orphaned blobs when site
// data is cleared.
class CONTENT_EXPORT CrossOriginStorageRegistry
    : public base::SupportsUserData::Data {
 public:
  // Returns the registry for `browser_context`, creating it on first use.
  // Returns nullptr if no storage location could be determined.
  static CrossOriginStorageRegistry* GetOrCreateForBrowserContext(
      BrowserContext* browser_context);

  explicit CrossOriginStorageRegistry(base::FilePath root_directory);
  CrossOriginStorageRegistry(const CrossOriginStorageRegistry&) = delete;
  CrossOriginStorageRegistry& operator=(const CrossOriginStorageRegistry&) =
      delete;
  ~CrossOriginStorageRegistry() override;

  using OpenForReadCallback =
      base::OnceCallback<void(CrossOriginStorageOpenResult)>;

  // Looks up the committed resource for `hash`, provided `caller` is allowed to
  // read it. Never creates an entry. When `update_access_time` is true, updates
  // the entry's `last_access_time` and persists metadata to disk. `caller` must
  // be the browser-determined origin, not one supplied by the renderer.
  void OpenForRead(const CrossOriginStorageHash& hash,
                   const url::Origin& caller,
                   bool update_access_time,
                   OpenForReadCallback callback);

  using CommitCallback = base::OnceCallback<void(CrossOriginStorageStatus)>;

  // Verifies the staging writer's digest against `hash` and, on match,
  // atomically moves the staging file into the content-addressed cache (or
  // deletes it if the blob already exists) and records `caller` as a storing
  // origin with the sharing scope in `access_spec`. The staging file is deleted
  // on any failure. `caller` must be the browser-determined origin, not one
  // supplied by the renderer.
  void Commit(const CrossOriginStorageHash& hash,
              const url::Origin& caller,
              CrossOriginStorageAccessSpec access_spec,
              CrossOriginStorageStagingWriter::Result staging_result,
              CommitCallback callback);

  using CreateStagingWriterCallback = base::OnceCallback<void(
      base::SequenceBound<CrossOriginStorageStagingWriter>)>;

  // Creates a quarantined staging file (never exposed to web readers) and
  // returns a `SequenceBound<CrossOriginStorageStagingWriter>` bound to a
  // dedicated blocking sequence so raw OS file handles never transit
  // `BrowserThread::UI` and cancelled requests clean up their staging file
  // automatically.
  //
  // Creation runs on the backend sequence so it is strictly ordered after the
  // startup staging directory sweep rather than racing it. Returns a null
  // `SequenceBound` on failure.
  void CreateStagingWriter(blink::mojom::CrossOriginStorageAlgorithm algorithm,
                           CreateStagingWriterCallback callback);

  base::WeakPtr<CrossOriginStorageRegistry> GetWeakPtr();

  // Synchronously flushes and destroys `backend_` on its background sequence
  // for tests that simulate browser restarts on the same profile directory.
  void SynchronouslyResetBackendForTesting();

 private:
  // Owns all disk and metadata state; lives on a blocking sequence.
  class StorageBackend;

  base::SequenceBound<StorageBackend> backend_;

  base::WeakPtrFactory<CrossOriginStorageRegistry> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CROSS_ORIGIN_STORAGE_CROSS_ORIGIN_STORAGE_REGISTRY_H_
