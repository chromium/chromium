// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLIENT_STATE_CHECKER_WRAPPER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLIENT_STATE_CHECKER_WRAPPER_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// This class provides a wrapper of the
// `mojo:Remote<storage::mojom::IndexedDBClientStateChecker>` so that the same
// remote can be shared across multiple `IndexedDBConnection`s that are created
// by the same client.
class CONTENT_EXPORT IndexedDBClientStateCheckerWrapper
    : public base::RefCounted<IndexedDBClientStateCheckerWrapper> {
 public:
  explicit IndexedDBClientStateCheckerWrapper(
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          client_state_checker_remote);

  IndexedDBClientStateCheckerWrapper(
      const IndexedDBClientStateCheckerWrapper&) = delete;
  IndexedDBClientStateCheckerWrapper& operator=(
      const IndexedDBClientStateCheckerWrapper&) = delete;

  void DisallowInactiveClient(
      storage::mojom::DisallowInactiveClientReason reason,
      mojo::PendingReceiver<storage::mojom::IndexedDBClientKeepActive>
          keep_active,
      storage::mojom::IndexedDBClientStateChecker::
          DisallowInactiveClientCallback callback);

 protected:
  virtual ~IndexedDBClientStateCheckerWrapper();

 private:
  friend class base::RefCounted<IndexedDBClientStateCheckerWrapper>;

  mojo::Remote<storage::mojom::IndexedDBClientStateChecker>
      client_state_checker_remote_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLIENT_STATE_CHECKER_WRAPPER_H_
