// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INDEXED_DB_CLIENT_STATE_CHECKER_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_INDEXED_DB_CLIENT_STATE_CHECKER_FACTORY_H_

#include <stddef.h>
#include <stdint.h>

#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

class CONTENT_EXPORT IndexedDBClientStateCheckerFactory {
 public:
  IndexedDBClientStateCheckerFactory() = delete;
  ~IndexedDBClientStateCheckerFactory() = delete;

  // Factory method that returns the `PendingRemote` bound to either a
  // `NoDocumentIndexedDBClientStateChecker` or a
  // `DocumentIndexedDBClientStateChecker` depending on the `rfh_id`.
  static mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
  InitializePendingRemote(const GlobalRenderFrameHostId& rfh_id);

  // Factory method that returns the pointer to the implementation of
  // `storage::mojom::IndexedDBClientStateChecker`. `rfh_id` should be a valid
  // one here.
  static storage::mojom::IndexedDBClientStateChecker*
  GetOrCreateIndexedDBClientStateCheckerForTesting(
      const GlobalRenderFrameHostId& rfh_id);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INDEXED_DB_CLIENT_STATE_CHECKER_FACTORY_H_
