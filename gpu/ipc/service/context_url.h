// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_CONTEXT_URL_H_
#define GPU_IPC_SERVICE_CONTEXT_URL_H_

#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "url/gurl.h"

namespace gpu {

// Wrapper for GURL identifying a context.
class GPU_IPC_SERVICE_EXPORT ContextUrl {
 public:
  // Sets the active URL crash key. This should be called when a context start
  // doing work so that GPU process crashes can be associated back to the active
  // context. This function is *not* thread safe and should only be called from
  // GPU main thread.
  //
  // Note this caches the hash of last URL used to set crash key and skips
  // setting crash key if |active_url| has the same hash.
  static void SetActiveUrl(const ContextUrl& active_url);

  explicit ContextUrl(GURL url);

  const GURL& url() const { return url_; }
  size_t hash() const { return url_hash_; }
  bool is_empty() const { return url_.is_empty(); }

 private:
  GURL url_;
  size_t url_hash_;
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_CONTEXT_URL_H_
