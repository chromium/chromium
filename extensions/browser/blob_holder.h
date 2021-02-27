// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BLOB_HOLDER_H_
#define EXTENSIONS_BROWSER_BLOB_HOLDER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/supports_user_data.h"

namespace content {
class BlobHandle;
class RenderProcessHost;
}

namespace extensions {

class ExtensionMessageFilter;

// Used for holding onto Blobs created into the browser process until a
// renderer takes over ownership. This class operates on the UI thread.
class BlobHolder : public base::SupportsUserData::Data {
 public:
  // Will create the BlobHolder if it doesn't already exist.
  static BlobHolder* FromRenderProcessHost(
      content::RenderProcessHost* render_process_host);

  ~BlobHolder() override;

  // Causes BlobHolder to take ownership of |blob|.
  void HoldBlobReference(std::unique_ptr<content::BlobHandle> blob);

 private:
  using BlobHandleMultimap =
      std::multimap<std::string, std::unique_ptr<content::BlobHandle>>;

  explicit BlobHolder(content::RenderProcessHost* render_process_host);

  // BlobHolder will drop a blob handle for each element in blob_uuids.
  // If caller wishes to drop multiple references to the same blob,
  // |blob_uuids| may contain duplicate UUIDs.
  void DropBlobs(const std::vector<std::string>& blob_uuids);
  friend class ExtensionMessageFilter;

  bool ContainsBlobHandle(content::BlobHandle* handle) const;

  // A reference to the owner of this class.
  content::RenderProcessHost* render_process_host_;

  BlobHandleMultimap held_blobs_;

  DISALLOW_COPY_AND_ASSIGN(BlobHolder);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BLOB_HOLDER_H_
