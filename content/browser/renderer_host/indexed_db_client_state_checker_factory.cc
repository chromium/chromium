// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/indexed_db_client_state_checker_factory.h"

#include <map>
#include <memory>
#include <tuple>

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "ipc/constants.mojom.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {
namespace {

using IndexedDBDisallowActivationReason = DisallowInactiveClientReason;

DisallowActivationReasonId ConvertToDisallowActivationReasonId(
    IndexedDBDisallowActivationReason reason) {
  switch (reason) {
    case IndexedDBDisallowActivationReason::kVersionChangeEvent:
      return DisallowActivationReasonId::kIndexedDBEvent;
    case IndexedDBDisallowActivationReason::kTransactionIsAcquiringLocks:
      return DisallowActivationReasonId::kIndexedDBTransactionIsAcquiringLocks;
    case IndexedDBDisallowActivationReason::
        kTransactionIsStartingWhileBlockingOthers:
      return DisallowActivationReasonId::
          kIndexedDBTransactionIsStartingWhileBlockingOthers;
    case IndexedDBDisallowActivationReason::
        kTransactionIsOngoingAndBlockingOthers:
      return DisallowActivationReasonId::
          kIndexedDBTransactionIsOngoingAndBlockingOthers;
  }
}

// This class should be used when the client has an associated document. The
// client checks are performed based on the document. This class extends
// `DocumentUserData` because a document has one client per IndexedDB connection
// to a database.
class DocumentIndexedDBClientStateChecker final
    : public DocumentUserData<DocumentIndexedDBClientStateChecker> {
 public:
  ~DocumentIndexedDBClientStateChecker() final = default;

  bool CheckIfClientWasActive(IndexedDBDisallowActivationReason reason) {
    bool was_active = false;

    if (render_frame_host().GetLifecycleState() ==
        RenderFrameHost::LifecycleState::kPrerendering) {
      // Page under prerendering is able to continue the JS execution so it
      // won't block the IndexedDB events. It shouldn't be deemed inactive for
      // the IndexedDB service.
      was_active = true;
    } else {
      // Call `IsInactiveAndDisallowActivation` to obtain the client state, this
      // also brings side effect like evicting the page if it's in back/forward
      // cache.
      was_active = !render_frame_host().IsInactiveAndDisallowActivation(
          ConvertToDisallowActivationReasonId(reason));
    }

    return was_active;
  }

  std::tuple<bool, ScopedKeepActive> DisallowInactiveClient(
      IndexedDBDisallowActivationReason reason) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // This client is currently blocking another client, for example because it
    // has a transaction that holds locks needed by the another client or
    // because it has a connection that prevents a version change in another
    // client. There are 2 situations that could prevent this client from
    // continuing its work and unblocking the other client: freezing and
    // back-forward cache. They are handled differently.
    //
    // In both cases, if the document is neither frozen nor in the back-forward
    // cache, there is nothing to do. If either situations happen in the future,
    // `DisallowInactiveClient()` will be called again for it and then take
    // action, by either unfreezing or evicting the document from the
    // back-forward cache.
    //
    // In the case of a frozen document, we register a
    // HoldingBlockingIDBLockHandle that will unfreeze and prevent the document
    // from being frozen for the lifetime of the handle.
    //
    // In the case the document is in the back-forward cache, the call to
    // `CheckIfClientWasActive()` below will evict it.
    //
    // In addition, if `reason` is kVersionChangeEvent, then we register both
    // a HoldingBlockingIDBLockHandle and a
    // BackForwardCacheDisablingFeatureHandle to prevent the document from going
    // into an inactive state until the IndexedDB connection is successfully
    // closed and the context is automatically destroyed.
    bool is_version_change_event =
        reason == IndexedDBDisallowActivationReason::kVersionChangeEvent;

    if (!CheckIfClientWasActive(reason)) {
      return {/*was_active=*/false, CreateNullScopedKeepActive()};
    }

    RenderFrameHostImpl* render_frame_host_impl =
        RenderFrameHostImpl::From(&render_frame_host());

    // If the client was in the BFCache, it should have been evicted with the
    // check above. Note that until crbug.com/40691610 is fixed, a
    // RenderFrameHost that is frozen for any other reasons than BFCache is
    // considered active.
    CHECK_NE(render_frame_host_impl->GetLifecycleState(),
             RenderFrameHost::LifecycleState::kInBackForwardCache);

    // If none of the 2 handle types has to be created, we don't even need to
    // bother with the keep-active.
    const bool create_holding_blocking_idb_lock_handle =
        render_frame_host_impl->IsFrozen() || is_version_change_event;
    const bool create_bfcache_feature_handle = is_version_change_event;
    if (!create_holding_blocking_idb_lock_handle &&
        !create_bfcache_feature_handle) {
      return {/*was_active=*/true, CreateNullScopedKeepActive()};
    }

    uint64_t context_id = next_context_id_++;
    KeepActiveReceiverContext& context = keep_active_contexts_[context_id];

    if (create_holding_blocking_idb_lock_handle) {
      context.holding_blocking_idb_lock_handle =
          render_frame_host_impl->RegisterHoldingBlockingIDBLockHandle();
    }

    if (create_bfcache_feature_handle) {
      context.bfcache_feature_handle =
          render_frame_host_impl
              ->RegisterBackForwardCacheDisablingNonStickyFeature(
                  blink::scheduler::WebSchedulerTrackedFeature::
                      kIndexedDBEvent);
    }

    ScopedKeepActive keep_active_handle(base::BindPostTask(
        GetUIThreadTaskRunner({}),
        base::BindOnce(
            &DocumentIndexedDBClientStateChecker::OnKeepActiveDisconnected,
            weak_factory_.GetWeakPtr(), context_id)));

    return {/*was_active=*/true, std::move(keep_active_handle)};
  }

 private:
  // Keep the association between the feature handles it registered.
  struct KeepActiveReceiverContext {
    BackForwardCacheDisablingFeatureHandle bfcache_feature_handle;
    RenderFrameHostImpl::HoldingBlockingIDBLockHandle
        holding_blocking_idb_lock_handle;
  };

  explicit DocumentIndexedDBClientStateChecker(RenderFrameHost* rfh)
      : DocumentUserData(rfh) {}

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  void OnKeepActiveDisconnected(uint64_t context_id) {
    keep_active_contexts_.erase(context_id);
  }

  uint64_t next_context_id_ = 1;
  std::map<uint64_t, KeepActiveReceiverContext> keep_active_contexts_;
  base::WeakPtrFactory<DocumentIndexedDBClientStateChecker> weak_factory_{this};
};

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(DocumentIndexedDBClientStateChecker);

// static
DisallowInactiveClientCallback
IndexedDBClientStateCheckerFactory::GetClientStateCheckerCallback() {
  return base::BindRepeating(
      [](int32_t process_id, blink::DocumentToken document_token,
         DisallowInactiveClientReason reason,
         DisallowInactiveClientResponseCallback callback) {
        GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(
                [](int32_t process_id, blink::DocumentToken document_token,
                   DisallowInactiveClientReason reason)
                    -> std::tuple<bool, ScopedKeepActive> {
                  RenderFrameHost* rfh = RenderFrameHostImpl::FromDocumentToken(
                      process_id, document_token);
                  if (!rfh) {
                    return {false, CreateNullScopedKeepActive()};
                  }
                  return DocumentIndexedDBClientStateChecker::
                      GetOrCreateForCurrentDocument(rfh)
                          ->DisallowInactiveClient(reason);
                },
                process_id, document_token, reason),
            std::move(callback));
      });
}

}  // namespace content
