// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/indexed_db_client_state_checker_factory.h"

#include <memory>

#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom-shared.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/disallow_activation_reason.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {
namespace {

using IndexedDBDisallowActivationReason =
    storage::mojom::DisallowInactiveClientReason;

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

// The class will only provide the default result and the client will be
// considered active. It should be used when the client doesn't have an
// associated document, as is the case for shared worker or service worker.
class NoDocumentIndexedDBClientStateChecker
    : public storage::mojom::IndexedDBClientStateChecker {
 public:
  NoDocumentIndexedDBClientStateChecker() = default;
  ~NoDocumentIndexedDBClientStateChecker() override = default;
  NoDocumentIndexedDBClientStateChecker(
      const NoDocumentIndexedDBClientStateChecker&) = delete;
  NoDocumentIndexedDBClientStateChecker& operator=(
      const NoDocumentIndexedDBClientStateChecker&) = delete;

  // storage::mojom::IndexedDBClientStateChecker overrides:
  // Non-document clients are always active, since the inactive state such as
  // back/forward cache is not applicable to them.
  void DisallowInactiveClient(
      storage::mojom::DisallowInactiveClientReason reason,
      mojo::PendingReceiver<storage::mojom::IndexedDBClientKeepActive>
          keep_active,
      DisallowInactiveClientCallback callback) override {
    std::move(callback).Run(/*was_active=*/true);
  }
  void MakeClone(
      mojo::PendingReceiver<storage::mojom::IndexedDBClientStateChecker>
          receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  mojo::ReceiverSet<storage::mojom::IndexedDBClientStateChecker> receivers_;
};

// This class should be used when the client has an associated document. The
// client checks are performed based on the document. This class extends
// `DocumentUserData` because a document has one client per IndexedDB connection
// to a database.
class DocumentIndexedDBClientStateChecker final
    : public DocumentUserData<DocumentIndexedDBClientStateChecker>,
      public storage::mojom::IndexedDBClientStateChecker,
      public storage::mojom::IndexedDBClientKeepActive {
 public:
  ~DocumentIndexedDBClientStateChecker() final = default;

  void Bind(mojo::PendingReceiver<storage::mojom::IndexedDBClientStateChecker>
                receiver) {
    receivers_.Add(this, std::move(receiver));
  }

  bool CheckIfClientWasActive(
      storage::mojom::DisallowInactiveClientReason reason) {
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

  // storage::mojom::IndexedDBClientStateChecker overrides:
  void DisallowInactiveClient(
      storage::mojom::DisallowInactiveClientReason reason,
      mojo::PendingReceiver<storage::mojom::IndexedDBClientKeepActive>
          keep_active,
      DisallowInactiveClientCallback callback) override {
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
        reason ==
        storage::mojom::DisallowInactiveClientReason::kVersionChangeEvent;

    CHECK(keep_active.is_valid());
    bool was_active = CheckIfClientWasActive(reason);
    if (!was_active) {
      std::move(callback).Run(was_active);
      return;
    }

    RenderFrameHostImpl* render_frame_host_impl =
        RenderFrameHostImpl::From(&render_frame_host());

    // If the client was in the BFCache, it should have been evicted with the
    // check above. Note that until crbug.com/40691610 is fixed, a
    // RenderFrameHost that is frozen for any other reasons than BFCache is
    // considered active.
    CHECK_NE(render_frame_host_impl->GetLifecycleState(),
             RenderFrameHost::LifecycleState::kInBackForwardCache);

    RenderFrameHostImpl::HoldingBlockingIDBLockHandle
        holding_blocking_idb_lock_handle;
    if (render_frame_host_impl->IsFrozen() || is_version_change_event) {
      holding_blocking_idb_lock_handle =
          render_frame_host_impl->RegisterHoldingBlockingIDBLockHandle();
    }

    RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle
        bfcache_feature_handle;
    if (is_version_change_event) {
      bfcache_feature_handle =
          render_frame_host_impl
              ->RegisterBackForwardCacheDisablingNonStickyFeature(
                  blink::scheduler::WebSchedulerTrackedFeature::
                      kIndexedDBEvent);
    }

    if (bfcache_feature_handle.IsValid() ||
        holding_blocking_idb_lock_handle.IsValid()) {
      KeepActiveReceiverContext context(
          std::move(bfcache_feature_handle),
          std::move(holding_blocking_idb_lock_handle));
      keep_active_receivers_.Add(this, std::move(keep_active),
                                 std::move(context));
    }

    std::move(callback).Run(was_active);
  }

  void MakeClone(
      mojo::PendingReceiver<storage::mojom::IndexedDBClientStateChecker>
          receiver) override {
    Bind(std::move(receiver));
  }

 private:
  // Keep the association between the receiver and the feature handles it
  // registered.
  class KeepActiveReceiverContext {
   public:
    KeepActiveReceiverContext() = default;
    KeepActiveReceiverContext(
        RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle
            bfcache_feature_handle,
        RenderFrameHostImpl::HoldingBlockingIDBLockHandle
            holding_blocking_idb_lock_handle)
        : bfcache_feature_handle_(std::move(bfcache_feature_handle)),
          holding_blocking_idb_lock_handle_(
              std::move(holding_blocking_idb_lock_handle)) {}
    KeepActiveReceiverContext(KeepActiveReceiverContext&& context) noexcept
        : bfcache_feature_handle_(std::move(context.bfcache_feature_handle_)),
          holding_blocking_idb_lock_handle_(
              std::move(context.holding_blocking_idb_lock_handle_)) {}

    ~KeepActiveReceiverContext() = default;

   private:
    RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle
        bfcache_feature_handle_;
    RenderFrameHostImpl::HoldingBlockingIDBLockHandle
        holding_blocking_idb_lock_handle_;
  };

  explicit DocumentIndexedDBClientStateChecker(RenderFrameHost* rfh)
      : DocumentUserData(rfh) {}

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  mojo::ReceiverSet<storage::mojom::IndexedDBClientStateChecker> receivers_;
  mojo::ReceiverSet<storage::mojom::IndexedDBClientKeepActive,
                    KeepActiveReceiverContext>
      keep_active_receivers_;
};

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(DocumentIndexedDBClientStateChecker);

// static
mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
IndexedDBClientStateCheckerFactory::InitializePendingRemote(
    const storage::BucketClientInfo& client_info) {
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      client_state_checker_remote;
  if (client_info.document_token) {
    RenderFrameHost* rfh = RenderFrameHostImpl::FromDocumentToken(
        client_info.process_id, client_info.document_token.value());
    CHECK(rfh);
    DocumentIndexedDBClientStateChecker::GetOrCreateForCurrentDocument(rfh)
        ->Bind(client_state_checker_remote.InitWithNewPipeAndPassReceiver());
  } else {
    if (client_info.context_token.Is<blink::SharedWorkerToken>() ||
        client_info.context_token.Is<blink::ServiceWorkerToken>()) {
      // Use a default checker instance for valid clients that have no
      // associated document. See comments on
      // `NoDocumentIndexedDBClientStateChecker`.
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<NoDocumentIndexedDBClientStateChecker>(),
          client_state_checker_remote.InitWithNewPipeAndPassReceiver());
    } else if (client_info.context_token.Is<blink::DedicatedWorkerToken>()) {
      // The rare case of a dedicated worker not having an associated document
      // can occur when the worker has outlived the parent RFH. See code comment
      // on `DedicatedWorkerHost`. We will not bind the remote in this case.
    } else {
      // No other client type is expected.
      NOTREACHED();
    }
  }
  return client_state_checker_remote;
}

// static
storage::mojom::IndexedDBClientStateChecker*
IndexedDBClientStateCheckerFactory::
    GetOrCreateIndexedDBClientStateCheckerForTesting(
        const GlobalRenderFrameHostId& rfh_id) {
  CHECK_NE(rfh_id.frame_routing_id, MSG_ROUTING_NONE)
      << "RFH id should be valid when testing";
  return DocumentIndexedDBClientStateChecker::GetOrCreateForCurrentDocument(
      RenderFrameHost::FromID(rfh_id));
}

}  // namespace content
