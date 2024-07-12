// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/indexed_db_client_state_checker_factory.h"

#include <memory>

#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom-shared.h"
#include "content/browser/buckets/bucket_context.h"
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
// associated RenderFrameHost, as is the case for shared worker or service
// worker. Also stores the DevTools token corresponding to the worker.
class NoDocumentIndexedDBClientStateChecker
    : public storage::mojom::IndexedDBClientStateChecker {
 public:
  explicit NoDocumentIndexedDBClientStateChecker(
      base::UnguessableToken dev_tools_token)
      : dev_tools_token_(dev_tools_token) {}
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
  void GetDevToolsToken(GetDevToolsTokenCallback callback) override {
    std::move(callback).Run(dev_tools_token_);
  }
  void MakeClone(
      mojo::PendingReceiver<storage::mojom::IndexedDBClientStateChecker>
          receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  base::UnguessableToken dev_tools_token_;
  mojo::ReceiverSet<storage::mojom::IndexedDBClientStateChecker> receivers_;
};

// This class should be used when the client has a RenderFrameHost associated so
// the client checks are performed based on the document held by the
// RenderFrameHost.
// This class extends `DocumentUserData` because a document has one client per
// IndexedDB connection to a database.
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
    bool was_active = CheckIfClientWasActive(reason);
    if (was_active && keep_active.is_valid()) {
      // This is the only reason that we need to prevent the client from
      // inactive state.
      CHECK_EQ(
          reason,
          storage::mojom::DisallowInactiveClientReason::kVersionChangeEvent);
      // If the document is active, we need to register a non sticky feature to
      // prevent putting it into BFCache until the IndexedDB connection is
      // successfully closed and the context is automatically destroyed.
      // Since `kClientEventIsTriggered` is the only reason that should be
      // passed to this function, the non-sticky feature will always be
      // `kIndexedDBEvent`.
      KeepActiveReceiverContext context(
          static_cast<RenderFrameHostImpl&>(render_frame_host())
              .RegisterBackForwardCacheDisablingNonStickyFeature(
                  blink::scheduler::WebSchedulerTrackedFeature::
                      kIndexedDBEvent));
      keep_active_receivers_.Add(this, std::move(keep_active),
                                 std::move(context));
    }

    std::move(callback).Run(was_active);
  }

  void GetDevToolsToken(GetDevToolsTokenCallback callback) override {
    std::move(callback).Run(
        static_cast<RenderFrameHostImpl&>(render_frame_host())
            .GetDevToolsToken());
  }

  void MakeClone(
      mojo::PendingReceiver<storage::mojom::IndexedDBClientStateChecker>
          receiver) override {
    Bind(std::move(receiver));
  }

  const base::UnguessableToken token() { return token_; }

 private:
  // Keep the association between the receiver and the feature handle it
  // registered.
  class KeepActiveReceiverContext {
   public:
    KeepActiveReceiverContext() = default;
    explicit KeepActiveReceiverContext(
        RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle handle)
        : feature_handle(std::move(handle)) {}
    KeepActiveReceiverContext(KeepActiveReceiverContext&& context) noexcept
        : feature_handle(std::move(context.feature_handle)) {}

    ~KeepActiveReceiverContext() = default;

   private:
    RenderFrameHostImpl::BackForwardCacheDisablingFeatureHandle feature_handle;
  };

  explicit DocumentIndexedDBClientStateChecker(RenderFrameHost* rfh)
      : DocumentUserData(rfh), token_(base::UnguessableToken::Create()) {}

  friend DocumentUserData;
  DOCUMENT_USER_DATA_KEY_DECL();

  // This token uniquely identifies `this`/the "client" to the other side of the
  // Mojo connection. It's used to prevent IDB code from over-zealously
  // disallowing BFCache for a render frame based on its own activity.
  base::UnguessableToken token_;

  mojo::ReceiverSet<storage::mojom::IndexedDBClientStateChecker> receivers_;
  mojo::ReceiverSet<storage::mojom::IndexedDBClientKeepActive,
                    KeepActiveReceiverContext>
      keep_active_receivers_;
};

}  // namespace

DOCUMENT_USER_DATA_KEY_IMPL(DocumentIndexedDBClientStateChecker);

// static
std::tuple<mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>,
           base::UnguessableToken>
IndexedDBClientStateCheckerFactory::InitializePendingRemote(
    BucketContext& bucket_context) {
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      client_state_checker_remote;
  if (GlobalRenderFrameHostId rfh_id =
          bucket_context.GetAssociatedRenderFrameHostId()) {
    RenderFrameHost* rfh = RenderFrameHost::FromID(rfh_id);
    if (!rfh) {
      // The rare case of the `RenderFrameHost` being null for a valid ID can
      // happen when the client is a dedicated worker and it has outlived the
      // parent RFH. See code comment on `DedicatedWorkerHost`.
      // Don't bind the remote in this case.
      return {std::move(client_state_checker_remote),
              base::UnguessableToken::Null()};
    }
    auto* checker =
        DocumentIndexedDBClientStateChecker::GetOrCreateForCurrentDocument(rfh);
    checker->Bind(client_state_checker_remote.InitWithNewPipeAndPassReceiver());
    return {std::move(client_state_checker_remote), checker->token()};
  }

  // If there is no `RenderFrameHost` associated with the client, use a default
  // checker instance for it.
  // See comments from `NoDocumentIndexedDBClientStateChecker`.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<NoDocumentIndexedDBClientStateChecker>(
          bucket_context.GetDevToolsToken()),
      client_state_checker_remote.InitWithNewPipeAndPassReceiver());
  return {std::move(client_state_checker_remote),
          base::UnguessableToken::Create()};
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
