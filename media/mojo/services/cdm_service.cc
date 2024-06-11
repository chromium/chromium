// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/cdm_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

namespace media {

namespace {

// Implementation of mojom::CdmFactory that creates and hosts MojoCdmServices
// which then host CDMs created by the media::CdmFactory provided by the
// CdmService::Client.
//
// Lifetime Note:
// 1. CdmFactoryImpl instances are owned by a DeferredDestroyUniqueReceiverSet
//    directly, which is owned by CdmService.
// 2. CdmFactoryImpl is destroyed in any of the following two cases:
//   - CdmService is destroyed. Because of (2) this should not happen except for
//     during browser shutdown, when the Cdservice could be destroyed directly,
//     ignoring any outstanding interface connections.
//   - mojo::CdmFactory disconnection happens, AND CdmFactoryImpl doesn't own
//     any CDMs (|cdm_receivers_| is empty). This is to prevent destroying the
//     CDMs too early (e.g. during page navigation) which could cause errors
//     (session closed) on the client side. See https://crbug.com/821171 for
//     details.
class CdmFactoryImpl final : public DeferredDestroy<mojom::CdmFactory> {
 public:
  CdmFactoryImpl(CdmService::Client* client,
                 mojo::PendingRemote<mojom::FrameInterfaceFactory> interfaces)
      : client_(client), interfaces_(std::move(interfaces)) {
    DVLOG(1) << __func__;

    // base::Unretained is safe because |cdm_receivers_| is owned by |this|. If
    // |this| is destructed, |cdm_receivers_| will be destructed as well and the
    // error handler should never be called.
    cdm_receivers_.set_disconnect_handler(base::BindRepeating(
        &CdmFactoryImpl::OnReceiverDisconnect, base::Unretained(this)));
  }

  CdmFactoryImpl(const CdmFactoryImpl&) = delete;
  CdmFactoryImpl operator=(const CdmFactoryImpl&) = delete;
  ~CdmFactoryImpl() final { DVLOG(1) << __func__; }

  // mojom::CdmFactory implementation.
  void CreateCdm(const CdmConfig& cdm_config,
                 CreateCdmCallback callback) final {
    DVLOG(2) << __func__;

    auto* cdm_factory = GetCdmFactory();
    if (!cdm_factory) {
      std::move(callback).Run(mojo::NullRemote(), nullptr,
                              CreateCdmStatus::kCdmFactoryCreationFailed);
      return;
    }

    auto mojo_cdm_service =
        std::make_unique<MojoCdmService>(&cdm_service_context_);
    auto* raw_mojo_cdm_service = mojo_cdm_service.get();
    DCHECK(!pending_mojo_cdm_services_.count(raw_mojo_cdm_service));
    pending_mojo_cdm_services_[raw_mojo_cdm_service] =
        std::move(mojo_cdm_service);
    raw_mojo_cdm_service->Initialize(
        cdm_factory, cdm_config,
        base::BindOnce(&CdmFactoryImpl::OnCdmServiceInitialized,
                       weak_ptr_factory_.GetWeakPtr(), raw_mojo_cdm_service,
                       std::move(callback)));
  }

  // DeferredDestroy<mojom::CdmFactory> implementation.
  void OnDestroyPending(base::OnceClosure destroy_cb) final {
    destroy_cb_ = std::move(destroy_cb);
    if (cdm_receivers_.empty())
      std::move(destroy_cb_).Run();
    // else the callback will be called when |cdm_receivers_| become empty.
  }

 private:
  media::CdmFactory* GetCdmFactory() {
    if (!cdm_factory_) {
      cdm_factory_ = client_->CreateCdmFactory(interfaces_.get());
      DLOG_IF(ERROR, !cdm_factory_) << "CdmFactory not available.";
    }
    return cdm_factory_.get();
  }

  void OnReceiverDisconnect() {
    if (destroy_cb_ && cdm_receivers_.empty())
      std::move(destroy_cb_).Run();
  }

  void OnCdmServiceInitialized(MojoCdmService* raw_mojo_cdm_service,
                               CreateCdmCallback callback,
                               mojom::CdmContextPtr cdm_context,
                               CreateCdmStatus status) {
    DCHECK(raw_mojo_cdm_service);

    // Remove pending MojoCdmService from the mapping in all cases.
    DCHECK(pending_mojo_cdm_services_.count(raw_mojo_cdm_service));
    auto mojo_cdm_service =
        std::move(pending_mojo_cdm_services_[raw_mojo_cdm_service]);
    pending_mojo_cdm_services_.erase(raw_mojo_cdm_service);

    if (!cdm_context) {
      std::move(callback).Run(mojo::NullRemote(), nullptr, status);
      return;
    }

    mojo::PendingRemote<mojom::ContentDecryptionModule> remote;
    cdm_receivers_.Add(std::move(mojo_cdm_service),
                       remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote), std::move(cdm_context),
                            CreateCdmStatus::kSuccess);
  }

  // Must be declared before the receivers below because the bound objects might
  // take a raw pointer of |cdm_service_context_| and assume it's always
  // available.
  MojoCdmServiceContext cdm_service_context_;

  raw_ptr<CdmService::Client> client_;
  mojo::Remote<mojom::FrameInterfaceFactory> interfaces_;
  mojo::UniqueReceiverSet<mojom::ContentDecryptionModule> cdm_receivers_;
  std::unique_ptr<media::CdmFactory> cdm_factory_;
  base::OnceClosure destroy_cb_;

  // MojoCdmServices pending initialization.
  std::map<MojoCdmService*, std::unique_ptr<MojoCdmService>>
      pending_mojo_cdm_services_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<CdmFactoryImpl> weak_ptr_factory_{this};
};

}  // namespace

CdmService::CdmService(std::unique_ptr<Client> client,
                       mojo::PendingReceiver<mojom::CdmService> receiver)
    : receiver_(this, std::move(receiver)), client_(std::move(client)) {
  DVLOG(1) << __func__;
  DCHECK(client_);
}

CdmService::~CdmService() {
  DVLOG(1) << __func__;
}

void CdmService::CreateCdmFactory(
    mojo::PendingReceiver<mojom::CdmFactory> receiver,
    mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces) {
  // Ignore receiver if service has already stopped.
  if (!client_)
    return;

  cdm_factory_receivers_.Add(std::make_unique<CdmFactoryImpl>(
                                 client_.get(), std::move(frame_interfaces)),
                             std::move(receiver));
}

}  // namespace media
