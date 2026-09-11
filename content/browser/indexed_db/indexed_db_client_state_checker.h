// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLIENT_STATE_CHECKER_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLIENT_STATE_CHECKER_H_

#include <stdint.h>

#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {

// Reason for checking the client document's active state and potentially
// disallowing it from becoming inactive (or evicting it if already inactive).
enum class DisallowInactiveClientReason {
  // Used when a new connection is being opened with a higher version
  // number, which triggers a version change event being sent to existing
  // connections.
  kVersionChangeEvent = 0,
  // Used when a transaction first attempts to acquire locks (disallowing
  // whatever client holds or requests that lock from becoming inactive).
  kTransactionIsAcquiringLocks = 1,
  // Used when a transaction starts while its client document is already
  // inactive. This evicts the document from BFCache since the transaction
  // won't be able to proceed.
  kTransactionIsStartingWhileBlockingOthers = 2,
  // Similar to above, but used when a transaction is ongoing and its client
  // document becomes inactive while blocking other clients.
  kTransactionIsOngoingAndBlockingOthers = 3,
  kMaxValue = kTransactionIsOngoingAndBlockingOthers,
};

using IndexedDBDisallowActivationReason = DisallowInactiveClientReason;

// Scoped handle returned to the caller when a client document is kept active.
// Holding an instance of this runner unfreezes the document and/or prevents it
// from entering inactive states (such as BFCache or page freeze) while alive.
// Destroying the runner ends the keep-active scope, allowing the document to
// become inactive again.
using ScopedKeepActive = base::ScopedClosureRunner;

inline ScopedKeepActive CreateNullScopedKeepActive() {
  return ScopedKeepActive();
}

// Callback invoked on the IndexedDB sequence with the result of a
// DisallowInactiveClient check:
// - `was_active`: true if the document was active (or running in prerendering).
//   If false, the document was inactive (e.g. in BFCache) and has been evicted
//   and disallowed from future activation.
// - `keep_active`: a scoped handle that keeps the document active for as long
//   as it is held by the caller. Null if no keep-active handle was needed.
using DisallowInactiveClientResponseCallback =
    base::OnceCallback<void(bool /*was_active*/,
                            ScopedKeepActive /*keep_active*/)>;

// Repeating callback provided by the browser environment to check the active
// state of an IndexedDB client document identified by `(process_id,
// document_token)`.
//
// In order to correctly handle IndexedDB interactions when clients may be
// inactive (e.g. in the Back/Forward Cache or frozen):
// 1. If the client document is active, it may be disallowed from entering
//    inactive states (e.g. BFCache / frozen) for the lifetime of the returned
//    `ScopedKeepActive`.
// 2. If the client document is in BFCache, it is evicted and disallowed from
//    being activated again in the future via
//    `RenderFrameHost::IsInactiveAndDisallowActivation()`.
//
// Summary of allowed client document state transitions:
// - In the default case:
//     active <-> inactive (BFCache / frozen)
//     active -> destroyed
//     inactive -> destroyed
// - While `ScopedKeepActive` is held:
//     active -> destroyed
//     (active -> inactive is disallowed)
// - If the document was already inactive when checked:
//     inactive -> destroyed
//     (inactive -> active is disallowed via BFCache eviction)
//
// Note: Non-document clients (e.g. dedicated/shared workers) cannot enter
// BFCache or freeze independently and are always active; their handling is
// bypassed before invoking this callback.
using DisallowInactiveClientCallback = base::RepeatingCallback<void(
    int32_t /*process_id*/,
    blink::DocumentToken /*document_token*/,
    DisallowInactiveClientReason /*reason*/,
    DisallowInactiveClientResponseCallback /*callback*/)>;

// Test helper returning a callback that always reports the client as active.
inline DisallowInactiveClientCallback
CreateAlwaysActiveClientStateCheckerForTesting() {
  return base::BindRepeating(
      [](int32_t, blink::DocumentToken, DisallowInactiveClientReason,
         DisallowInactiveClientResponseCallback callback) {
        std::move(callback).Run(/*was_active=*/true,
                                CreateNullScopedKeepActive());
      });
}

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CLIENT_STATE_CHECKER_H_
