// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_worklet_manager.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/strings/strcat.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_shared_storage_host.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/subresource_url_authorizations.h"
#include "content/browser/interest_group/subresource_url_builder.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/auction_shared_storage_host.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

auction_worklet::mojom::AuctionWorkletPermissionsPolicyStatePtr
GetAuctionWorkletPermissionsPolicyState(RenderFrameHostImpl* auction_runner_rfh,
                                        const GURL& worklet_script_url) {
  const blink::PermissionsPolicy* permissions_policy =
      auction_runner_rfh->permissions_policy();

  url::Origin worklet_origin = url::Origin::Create(worklet_script_url);

  return auction_worklet::mojom::AuctionWorkletPermissionsPolicyState::New(
      permissions_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::kPrivateAggregation,
          worklet_origin),
      permissions_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::kSharedStorage,
          worklet_origin));
}

}  // namespace

class AuctionWorkletManager::WorkletOwner
    : public base::RefCounted<AuctionWorkletManager::WorkletOwner> {
 public:
  // On construction, attempts to immediately create a worklet. If that
  // succeeds, it's up to the AuctionWorkletManager itself to inform the caller
  // a worklet was synchronously created. Otherwise, the WorkletOwner will
  // immediately start waiting for a process to be available, and once one is,
  // create a worklet, informing all associated WorkletHandles.
  WorkletOwner(AuctionWorkletManager* worklet_manager,
               WorkletInfo worklet_info);

  // Registers/unregisters a WorkletHandle for the worklet `this` owns.
  void RegisterHandle(WorkletHandle* handle) { handles_.AddObserver(handle); }
  void UnregisterHandle(WorkletHandle* handle) {
    handles_.RemoveObserver(handle);
  }

  auction_worklet::mojom::BidderWorklet* bidder_worklet() {
    DCHECK(bidder_worklet_);
    return bidder_worklet_.get();
  }

  auction_worklet::mojom::SellerWorklet* seller_worklet() {
    DCHECK(seller_worklet_);
    return seller_worklet_.get();
  }

  const WorkletInfo& worklet_info() const { return worklet_info_; }

  // Whether or not a worklet has been created. Once a worklet has been created,
  // always returns true, even after disconnect or error.
  bool worklet_created() const {
    return bidder_worklet_.is_bound() || seller_worklet_.is_bound();
  }

  SubresourceUrlAuthorizations* subresource_url_authorizations() {
    if (!url_loader_factory_proxy_)
      return nullptr;
    return &url_loader_factory_proxy_->subresource_url_authorizations();
  }

 private:
  friend class base::RefCounted<WorkletOwner>;

  ~WorkletOwner();

  // Called if the worklet becomes unusable. This happens on destruction (once
  // all refs have been released) or when the Mojo pipe is closed. Removes
  // `this` from `worklet_manager_`, so that future requests using the same
  // `worklet_info` will trigger creation of a new worklet, rather than trying
  // to use the unusable one.
  void WorkletNoLongerUsable();

  // Called once the AuctionProcessManager provides a process to load a worklet
  // in. Immediately loads the worklet and informs WorkletHandles.
  void OnProcessAssigned();

  // Mojo disconnect with reason handler. If there's a description, it's a load
  // error. Otherwise, it's a crash. Passes error information on to all
  // associated WorkletHandles.
  void OnWorkletDisconnected(uint32_t /* custom_reason */,
                             const std::string& description);

  // Set to null once `this` is removed from AuctionWorkletManager's
  // WorkletOwner list, which happens on destruction or on Mojo pipe closure.
  // The latter allows a handle to still exist and refer to a WorkletOwner with
  // a broken Worklet pipe, while new requests for the same worklet will result
  // in creating a fresh Mojo worklet.
  raw_ptr<AuctionWorkletManager> worklet_manager_;

  const WorkletInfo worklet_info_;

  AuctionProcessManager::ProcessHandle process_handle_;

  // List of live WorkletHandles to notify in case a Worklet is created or on
  // Mojo pipe closure.
  base::ObserverList<WorkletHandle, /*check_empty=*/true> handles_;

  std::unique_ptr<AuctionURLLoaderFactoryProxy> url_loader_factory_proxy_;
  mojo::Remote<auction_worklet::mojom::BidderWorklet> bidder_worklet_;
  mojo::Remote<auction_worklet::mojom::SellerWorklet> seller_worklet_;
  // This must be destroyed before the worklet it's passed, since it hangs on to
  // a raw pointer to it.
  std::unique_ptr<DebuggableAuctionWorklet> worklet_debug_;
};

AuctionWorkletManager::WorkletOwner::WorkletOwner(
    AuctionWorkletManager* worklet_manager,
    WorkletInfo worklet_info)
    : worklet_manager_(worklet_manager),
      worklet_info_(std::move(worklet_info)) {
  if (worklet_manager_->auction_process_manager()->RequestWorkletService(
          worklet_info_.type, url::Origin::Create(worklet_info_.script_url),
          worklet_manager_->delegate()->GetFrameSiteInstance(),
          &process_handle_,
          base::BindOnce(
              &AuctionWorkletManager::WorkletOwner::OnProcessAssigned,
              base::Unretained(this)))) {
    OnProcessAssigned();
  }
}

AuctionWorkletManager::WorkletOwner::~WorkletOwner() {
  WorkletNoLongerUsable();
}

void AuctionWorkletManager::WorkletOwner::WorkletNoLongerUsable() {
  if (worklet_manager_) {
    worklet_manager_->OnWorkletNoLongerUsable(this);
    worklet_manager_ = nullptr;
  }
}

void AuctionWorkletManager::WorkletOwner::OnProcessAssigned() {
  DCHECK(!bidder_worklet_.is_bound());
  DCHECK(!seller_worklet_.is_bound());

  Delegate* delegate = worklet_manager_->delegate();
  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  RenderFrameHostImpl* const rfh = delegate->GetFrame();
  url_loader_factory_proxy_ = std::make_unique<AuctionURLLoaderFactoryProxy>(
      url_loader_factory.InitWithNewPipeAndPassReceiver(),
      base::BindRepeating(&Delegate::GetFrameURLLoaderFactory,
                          base::Unretained(delegate)),
      base::BindRepeating(&Delegate::GetTrustedURLLoaderFactory,
                          base::Unretained(delegate)),
      base::BindOnce(&Delegate::PreconnectSocket, base::Unretained(delegate)),
      worklet_manager_->top_window_origin(), worklet_manager_->frame_origin(),
      // NOTE: `rfh` can be null in tests.
      /*renderer_process_id=*/
      rfh ? absl::optional<int>(rfh->GetProcess()->GetID()) : absl::nullopt,
      /*is_for_seller_=*/worklet_info_.type == WorkletType::kSeller,
      delegate->GetClientSecurityState(), worklet_info_.script_url,
      worklet_info_.wasm_url, worklet_info_.signals_url);

  switch (worklet_info_.type) {
    case WorkletType::kBidder: {
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          worklet_receiver = bidder_worklet_.BindNewPipeAndPassReceiver();
      worklet_debug_ = base::WrapUnique(new DebuggableAuctionWorklet(
          delegate->GetFrame(), &process_handle_, worklet_info_.script_url,
          bidder_worklet_.get()));
      process_handle_.GetService()->LoadBidderWorklet(
          std::move(worklet_receiver),
          worklet_manager_->MaybeBindAuctionSharedStorageHost(
              delegate->GetFrame(),
              url::Origin::Create(worklet_info_.script_url)),
          worklet_debug_->should_pause_on_start(),
          std::move(url_loader_factory), worklet_info_.script_url,
          worklet_info_.wasm_url, worklet_info_.signals_url,
          worklet_manager_->top_window_origin(),
          GetAuctionWorkletPermissionsPolicyState(delegate->GetFrame(),
                                                  worklet_info_.script_url),
          worklet_info_.experiment_group_id.has_value(),
          worklet_info_.experiment_group_id.value_or(0u));
      bidder_worklet_.set_disconnect_with_reason_handler(base::BindOnce(
          &WorkletOwner::OnWorkletDisconnected, base::Unretained(this)));
      break;
    }

    case WorkletType::kSeller: {
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          worklet_receiver = seller_worklet_.BindNewPipeAndPassReceiver();
      worklet_debug_ = base::WrapUnique(new DebuggableAuctionWorklet(
          delegate->GetFrame(), &process_handle_, worklet_info_.script_url,
          seller_worklet_.get()));
      process_handle_.GetService()->LoadSellerWorklet(
          std::move(worklet_receiver),
          worklet_manager_->MaybeBindAuctionSharedStorageHost(
              delegate->GetFrame(),
              url::Origin::Create(worklet_info_.script_url)),
          worklet_debug_->should_pause_on_start(),
          std::move(url_loader_factory), worklet_info_.script_url,
          worklet_info_.signals_url, worklet_manager_->top_window_origin(),
          GetAuctionWorkletPermissionsPolicyState(delegate->GetFrame(),
                                                  worklet_info_.script_url),
          worklet_info_.experiment_group_id.has_value(),
          worklet_info_.experiment_group_id.value_or(0u));
      seller_worklet_.set_disconnect_with_reason_handler(base::BindOnce(
          &WorkletOwner::OnWorkletDisconnected, base::Unretained(this)));
      break;
    }
  }

  for (auto& handle : handles_) {
    handle.OnWorkletAvailable();
  }
}

void AuctionWorkletManager::WorkletOwner::OnWorkletDisconnected(
    uint32_t /* custom_reason */,
    const std::string& description) {
  // This may only be invoked once per worklet, and `worklet_manager_` is only
  // cleared by this method and on destruction, so it should not be null.
  DCHECK(worklet_manager_);

  WorkletNoLongerUsable();

  FatalErrorType error_type;
  std::vector<std::string> errors;
  // If there's a description, it's a load failure. Otherwise, it's a crash.
  if (!description.empty()) {
    error_type = FatalErrorType::kScriptLoadFailed;
    errors.push_back(description);
  } else {
    error_type = FatalErrorType::kWorkletCrash;
    errors.emplace_back(
        base::StrCat({worklet_info_.script_url.spec(), " crashed."}));
  }

  for (auto& handle : handles_) {
    handle.OnFatalError(error_type, errors);
  }
}

AuctionWorkletManager::WorkletHandle::~WorkletHandle() {
  // We register with subresource_url_authorizations() only if
  // AuthorizeSubresourceUrls() was called, so deregister if that's the case.
  //
  // This also must imply that the object should exist --- the proxy that owns
  // the SubresourceUrlAuthorizations gets created when a process is assigned,
  // and that's a precondition for calling AuthorizeSubresourceUrls().
  if (authorized_subresources_) {
    DCHECK(worklet_owner_->subresource_url_authorizations());
    worklet_owner_->subresource_url_authorizations()
        ->OnWorkletHandleDestruction(this);
  }
  worklet_owner_->UnregisterHandle(this);
}

auction_worklet::mojom::BidderWorklet*
AuctionWorkletManager::WorkletHandle::GetBidderWorklet() {
  DCHECK_EQ(WorkletType::kBidder, worklet_owner_->worklet_info().type);
  DCHECK(worklet_owner_->bidder_worklet());
  return worklet_owner_->bidder_worklet();
}

auction_worklet::mojom::SellerWorklet*
AuctionWorkletManager::WorkletHandle::GetSellerWorklet() {
  DCHECK_EQ(WorkletType::kSeller, worklet_owner_->worklet_info().type);
  DCHECK(worklet_owner_->seller_worklet());
  return worklet_owner_->seller_worklet();
}

const SubresourceUrlAuthorizations& AuctionWorkletManager::WorkletHandle::
    GetSubresourceUrlAuthorizationsForTesting() {
  DCHECK(authorized_subresources_);
  DCHECK(worklet_owner_->subresource_url_authorizations());
  return *worklet_owner_->subresource_url_authorizations();
}

AuctionWorkletManager::WorkletHandle::WorkletHandle(
    scoped_refptr<WorkletOwner> worklet_owner,
    base::OnceClosure worklet_available_callback,
    FatalErrorCallback fatal_error_callback)
    : worklet_owner_(std::move(worklet_owner)),
      worklet_available_callback_(std::move(worklet_available_callback)),
      fatal_error_callback_(std::move(fatal_error_callback)) {
  DCHECK(worklet_available_callback_);
  DCHECK(fatal_error_callback_);

  // Delete `worklet_available_callback_` if worklet is already available, since
  // it won't be needed.
  if (worklet_owner_->worklet_created()) {
    worklet_available_callback_.Reset();
  }

  worklet_owner_->RegisterHandle(this);
}

void AuctionWorkletManager::WorkletHandle::OnWorkletAvailable() {
  DCHECK(worklet_available_callback_);
  std::move(worklet_available_callback_).Run();
}

void AuctionWorkletManager::WorkletHandle::OnFatalError(
    FatalErrorType type,
    const std::vector<std::string>& errors) {
  DCHECK(fatal_error_callback_);
  std::move(fatal_error_callback_).Run(type, errors);
}

void AuctionWorkletManager::WorkletHandle::AuthorizeSubresourceUrls(
    const SubresourceUrlBuilder& subresource_url_builder) {
  if (authorized_subresources_) {
    return;
  }

  authorized_subresources_ = true;

  DCHECK(worklet_owner_->subresource_url_authorizations());
  std::vector<SubresourceUrlBuilder::BundleSubresourceInfo>
      authorized_subresource_urls;
  if (subresource_url_builder.auction_signals()) {
    authorized_subresource_urls.push_back(
        *subresource_url_builder.auction_signals());
  }
  switch (worklet_owner_->worklet_info().type) {
    case WorkletType::kBidder: {
      const url::Origin bidder_origin =
          url::Origin::Create(worklet_owner_->worklet_info().script_url);
      auto it = subresource_url_builder.per_buyer_signals().find(bidder_origin);
      if (it != subresource_url_builder.per_buyer_signals().end()) {
        authorized_subresource_urls.push_back(it->second);
      }
      break;
    }
    case WorkletType::kSeller: {
      if (subresource_url_builder.seller_signals()) {
        authorized_subresource_urls.push_back(
            *subresource_url_builder.seller_signals());
      }
      break;
    }
  }
  worklet_owner_->subresource_url_authorizations()->AuthorizeSubresourceUrls(
      this, std::move(authorized_subresource_urls));
}

bool AuctionWorkletManager::WorkletHandle::worklet_created() const {
  return worklet_owner_->worklet_created();
}

AuctionWorkletManager::AuctionWorkletManager(
    AuctionProcessManager* auction_process_manager,
    url::Origin top_window_origin,
    url::Origin frame_origin,
    Delegate* delegate)
    : auction_process_manager_(auction_process_manager),
      top_window_origin_(std::move(top_window_origin)),
      frame_origin_(std::move(frame_origin)),
      delegate_(delegate) {
  if (base::FeatureList::IsEnabled(blink::features::kSharedStorageAPI)) {
    auction_shared_storage_host_ = std::make_unique<AuctionSharedStorageHost>(
        static_cast<StoragePartitionImpl*>(
            delegate_->GetFrame()->GetProcess()->GetStoragePartition())
            ->GetSharedStorageManager());
  }
}

AuctionWorkletManager::~AuctionWorkletManager() = default;

bool AuctionWorkletManager::WorkletInfo::WorkletInfo::operator<(
    const WorkletInfo& other) const {
  return std::tie(type, script_url, wasm_url, signals_url,
                  experiment_group_id) <
         std::tie(other.type, other.script_url, other.wasm_url,
                  other.signals_url, other.experiment_group_id);
}

bool AuctionWorkletManager::RequestBidderWorklet(
    const GURL& bidding_logic_url,
    const absl::optional<GURL>& wasm_url,
    const absl::optional<GURL>& trusted_bidding_signals_url,
    absl::optional<uint16_t> experiment_group_id,
    base::OnceClosure worklet_available_callback,
    FatalErrorCallback fatal_error_callback,
    std::unique_ptr<WorkletHandle>& out_worklet_handle) {
  DCHECK(!out_worklet_handle);

  WorkletInfo worklet_info(WorkletType::kBidder,
                           /*script_url=*/bidding_logic_url, wasm_url,
                           /*signals_url=*/trusted_bidding_signals_url,
                           trusted_bidding_signals_url.has_value()
                               ? experiment_group_id
                               : absl::nullopt);
  return RequestWorkletInternal(
      std::move(worklet_info), std::move(worklet_available_callback),
      std::move(fatal_error_callback), out_worklet_handle);
}

bool AuctionWorkletManager::RequestSellerWorklet(
    const GURL& decision_logic_url,
    const absl::optional<GURL>& trusted_scoring_signals_url,
    absl::optional<uint16_t> experiment_group_id,
    base::OnceClosure worklet_available_callback,
    FatalErrorCallback fatal_error_callback,
    std::unique_ptr<WorkletHandle>& out_worklet_handle) {
  DCHECK(!out_worklet_handle);

  WorkletInfo worklet_info(WorkletType::kSeller,
                           /*script_url=*/decision_logic_url,
                           /*wasm_url=*/absl::nullopt,
                           /*signals_url=*/trusted_scoring_signals_url,
                           experiment_group_id);
  return RequestWorkletInternal(
      std::move(worklet_info), std::move(worklet_available_callback),
      std::move(fatal_error_callback), out_worklet_handle);
}

AuctionWorkletManager::WorkletInfo::WorkletInfo(
    WorkletType type,
    const GURL& script_url,
    const absl::optional<GURL>& wasm_url,
    const absl::optional<GURL>& signals_url,
    absl::optional<uint16_t> experiment_group_id)
    : type(type),
      script_url(script_url),
      wasm_url(wasm_url),
      signals_url(signals_url),
      experiment_group_id(experiment_group_id) {}

AuctionWorkletManager::WorkletInfo::WorkletInfo(const WorkletInfo&) = default;
AuctionWorkletManager::WorkletInfo::WorkletInfo(WorkletInfo&&) = default;
AuctionWorkletManager::WorkletInfo::~WorkletInfo() = default;

bool AuctionWorkletManager::RequestWorkletInternal(
    WorkletInfo worklet_info,
    base::OnceClosure worklet_available_callback,
    FatalErrorCallback fatal_error_callback,
    std::unique_ptr<WorkletHandle>& out_worklet_handle) {
  auto worklet_it = worklets_.find(worklet_info);
  scoped_refptr<WorkletOwner> worklet;
  if (worklet_it != worklets_.end()) {
    worklet = worklet_it->second;
  } else {
    // Can't just insert in the map and put a reference in `worklet_it`, since
    // need to keep a live reference.
    worklet = base::MakeRefCounted<WorkletOwner>(this, worklet_info);
    worklets_.emplace(std::pair(std::move(worklet_info), worklet.get()));
  }
  out_worklet_handle.reset(new WorkletHandle(
      std::move(worklet), std::move(worklet_available_callback),
      std::move(fatal_error_callback)));
  return out_worklet_handle->worklet_created();
}

void AuctionWorkletManager::OnWorkletNoLongerUsable(WorkletOwner* worklet) {
  DCHECK(worklets_.count(worklet->worklet_info()));
  DCHECK_EQ(worklet, worklets_[worklet->worklet_info()]);

  worklets_.erase(worklet->worklet_info());
}

mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost>
AuctionWorkletManager::MaybeBindAuctionSharedStorageHost(
    RenderFrameHostImpl* auction_runner_rfh,
    const url::Origin& worklet_origin) {
  mojo::PendingRemote<auction_worklet::mojom::AuctionSharedStorageHost> remote;

  const blink::PermissionsPolicy* permissions_policy =
      auction_runner_rfh->permissions_policy();

  if (auction_shared_storage_host_ &&
      permissions_policy->IsFeatureEnabledForOrigin(
          blink::mojom::PermissionsPolicyFeature::kSharedStorage,
          worklet_origin)) {
    auction_shared_storage_host_->BindNewReceiver(
        worklet_origin, remote.InitWithNewPipeAndPassReceiver());
  }

  return remote;
}

}  // namespace content
