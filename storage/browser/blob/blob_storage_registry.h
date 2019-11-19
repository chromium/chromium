// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_
#define STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/blob/blob_storage_constants.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"

class GURL;

namespace storage {
class BlobEntry;

// This class stores the blob data in the various states of construction, as
// well as URL mappings to blob uuids.
// Implementation notes:
// * When removing a uuid registration, we do not check for URL mappings to that
//   uuid. The user must keep track of these.
class COMPONENT_EXPORT(STORAGE_BROWSER) BlobStorageRegistry {
 public:
  BlobStorageRegistry();
  ~BlobStorageRegistry();

  // Creates the blob entry with a refcount of 1 and a state of PENDING. If
  // the blob is already in use, we return null.
  BlobEntry* CreateEntry(const std::string& uuid,
                         const std::string& content_type,
                         const std::string& content_disposition);

  // Removes the blob entry with the given uuid. This does not unmap any
  // URLs that are pointing to this uuid. Returns if the entry existed.
  bool DeleteEntry(const std::string& uuid);

  bool HasEntry(const std::string& uuid) const;

  // Gets the blob entry for the given uuid. Returns nullptr if the entry
  // does not exist.
  BlobEntry* GetEntry(const std::string& uuid);
  const BlobEntry* GetEntry(const std::string& uuid) const;

  // Creates a url mapping from blob to the given url. Returns false if
  // there already is a map for the URL.
  bool CreateUrlMapping(const GURL& url,
                        mojo::PendingRemote<blink::mojom::Blob> blob);

  // Removes the given URL mapping. Returns false if the url wasn't mapped.
  bool DeleteURLMapping(const GURL& url);

  // Returns if the url is mapped to a blob.
  bool IsURLMapped(const GURL& blob_url) const;

  // Returns the blob from the given url. Returns a null remote if the mapping
  // doesn't exist.
  mojo::PendingRemote<blink::mojom::Blob> GetBlobFromURL(const GURL& url);

  size_t blob_count() const { return blob_map_.size(); }
  size_t url_count() const { return url_to_blob_.size(); }

  void AddTokenMapping(const base::UnguessableToken& token,
                       const GURL& url,
                       mojo::PendingRemote<blink::mojom::Blob> blob);
  void RemoveTokenMapping(const base::UnguessableToken& token);
  bool GetTokenMapping(const base::UnguessableToken& token,
                       GURL* url,
                       mojo::PendingRemote<blink::mojom::Blob>* blob) const;

 private:
  friend class ViewBlobInternalsJob;

  std::unordered_map<std::string, std::unique_ptr<BlobEntry>> blob_map_;
  std::map<GURL, mojo::Remote<blink::mojom::Blob>> url_to_blob_;
  std::map<base::UnguessableToken,
           std::pair<GURL, mojo::Remote<blink::mojom::Blob>>>
      token_to_url_and_blob_;

  DISALLOW_COPY_AND_ASSIGN(BlobStorageRegistry);
};

}  // namespace storage
#endif  // STORAGE_BROWSER_BLOB_BLOB_STORAGE_REGISTRY_H_
