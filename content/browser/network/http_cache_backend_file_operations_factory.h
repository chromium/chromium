// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_HTTP_CACHE_BACKEND_FILE_OPERATIONS_FACTORY_H_
#define CONTENT_BROWSER_NETWORK_HTTP_CACHE_BACKEND_FILE_OPERATIONS_FACTORY_H_

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/http_cache_backend_file_operations.mojom.h"

namespace content {

// A network::mojom::HttpCacheBackendFileOperationsFactory that creates
// network::mojom::HttpCacheBackendFileOperations that run file operations
// on the browser process.
class CONTENT_EXPORT HttpCacheBackendFileOperationsFactory final
    : public network::mojom::HttpCacheBackendFileOperationsFactory {
 public:
  // All the operations must be performed under `path`.
  explicit HttpCacheBackendFileOperationsFactory(const base::FilePath& path);
  ~HttpCacheBackendFileOperationsFactory() override;

  void Create(
      mojo::PendingReceiver<network::mojom::HttpCacheBackendFileOperations>
          receiver) override;

 private:
  const base::FilePath path_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_HTTP_CACHE_BACKEND_FILE_OPERATIONS_FACTORY_H_
