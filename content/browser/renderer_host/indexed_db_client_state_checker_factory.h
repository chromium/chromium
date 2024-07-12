// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INDEXED_DB_CLIENT_STATE_CHECKER_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_INDEXED_DB_CLIENT_STATE_CHECKER_FACTORY_H_

#include <stddef.h>
#include <stdint.h>

#include "base/unguessable_token.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

class BucketContext;

class CONTENT_EXPORT IndexedDBClientStateCheckerFactory {
 public:
  IndexedDBClientStateCheckerFactory() = delete;
  ~IndexedDBClientStateCheckerFactory() = delete;

  // Factory method that creates and returns a client state checker and a token
  // that serves as a unique identifier for the `RenderFrameHost` associated
  // with `bucket_context`. Callers must check the validity of the returned
  // `PendingRemote` before consuming it since it will be bound only if
  // `bucket_context` is in a valid state. This method is called on the browser
  // UI thread and the objects it returns are suitable for use from other
  // (privileged) threads or processes.
  // TODO (crbug.com/349019967): Return a strongly-typed token from Blink's
  // tokens.h here instead of a custom, generated token.
  static std::tuple<
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>,
      base::UnguessableToken>
  InitializePendingRemote(BucketContext& bucket_context);

  // Factory method that returns the pointer to the implementation of
  // `storage::mojom::IndexedDBClientStateChecker`. `rfh_id` should be a valid
  // one here.
  static storage::mojom::IndexedDBClientStateChecker*
  GetOrCreateIndexedDBClientStateCheckerForTesting(
      const GlobalRenderFrameHostId& rfh_id);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INDEXED_DB_CLIENT_STATE_CHECKER_FACTORY_H_
