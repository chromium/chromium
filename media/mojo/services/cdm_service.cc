// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/cdm_service.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/cdm/cdm_module.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"

#if defined(OS_MAC)
#include <vector>
#include "sandbox/mac/seatbelt_extension.h"
#endif  // defined(OS_MAC)

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

  ~CdmFactoryImpl() final { DVLOG(1) << __func__; }

  // mojom::CdmFactory implementation.
  void CreateCdm(const std::string& key_system,
                 const CdmConfig& cdm_config,
                 CreateCdmCallback callback) final {
    DVLOG(2) << __func__;

    auto* cdm_factory = GetCdmFactory();
    if (!cdm_factory) {
      std::move(callback).Run(mojo::NullRemote(), base::nullopt,
                              mojo::NullRemote(),
                              "CDM Factory creation failed");
      return;
    }

    MojoCdmService::Create(
        cdm_factory, &cdm_service_context_, key_system, cdm_config,
        base::BindOnce(&CdmFactoryImpl::OnCdmServiceCreated,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  // DeferredDestroy<mojom::CdmFactory> implemenation.
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

  void OnCdmServiceCreated(CreateCdmCallback callback,
                           std::unique_ptr<MojoCdmService> cdm_service,
                           mojo::PendingRemote<mojom::Decryptor> decryptor,
                           const std::string& error_message) {
    if (!cdm_service) {
      std::move(callback).Run(mojo::NullRemote(), base::nullopt,
                              mojo::NullRemote(), error_message);
      return;
    }

    auto cdm_id = cdm_service->cdm_id();
    mojo::PendingRemote<mojom::ContentDecryptionModule> remote;
    cdm_receivers_.Add(std::move(cdm_service),
                       remote.InitWithNewPipeAndPassReceiver());
    std::move(callback).Run(std::move(remote), cdm_id, std::move(decryptor),
                            "");
  }

  // Must be declared before the receivers below because the bound objects might
  // take a raw pointer of |cdm_service_context_| and assume it's always
  // available.
  MojoCdmServiceContext cdm_service_context_;

  CdmService::Client* client_;
  mojo::Remote<mojom::FrameInterfaceFactory> interfaces_;
  mojo::UniqueReceiverSet<mojom::ContentDecryptionModule> cdm_receivers_;
  std::unique_ptr<media::CdmFactory> cdm_factory_;
  base::OnceClosure destroy_cb_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<CdmFactoryImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CdmFactoryImpl);
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

#if defined(OS_MAC)
void CdmService::LoadCdm(
    const base::FilePath& cdm_path,
    mojo::PendingRemote<mojom::SeatbeltExtensionTokenProvider> token_provider) {
#else
void CdmService::LoadCdm(const base::FilePath& cdm_path) {
#endif  // defined(OS_MAC)
  DVLOG(1) << __func__ << ": cdm_path = " << cdm_path.value();

  // Ignore request if service has already stopped.
  if (!client_)
    return;

  CdmModule* instance = CdmModule::GetInstance();
  if (instance->was_initialize_called()) {
    DCHECK_EQ(cdm_path, instance->GetCdmPath());
    return;
  }

#if defined(OS_MAC)
  std::vector<std::unique_ptr<sandbox::SeatbeltExtension>> extensions;

  if (token_provider) {
    std::vector<sandbox::SeatbeltExtensionToken> tokens;
    CHECK(mojo::Remote<mojom::SeatbeltExtensionTokenProvider>(
              std::move(token_provider))
              ->GetTokens(&tokens));

    for (auto&& token : tokens) {
      DVLOG(3) << "token: " << token.token();
      auto extension = sandbox::SeatbeltExtension::FromToken(std::move(token));
      if (!extension->Consume()) {
        DVLOG(1) << "Failed to consume sandbox seatbelt extension. This could "
                    "happen if --no-sandbox is specified.";
      }
      extensions.push_back(std::move(extension));
    }
  }
#endif  // defined(OS_MAC)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  std::vector<CdmHostFilePath> cdm_host_file_paths;
  client_->AddCdmHostFilePaths(&cdm_host_file_paths);
  bool success = instance->Initialize(cdm_path, cdm_host_file_paths);
#else
  bool success = instance->Initialize(cdm_path);
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  // This may trigger the sandbox to be sealed.
  client_->EnsureSandboxed();

#if defined(OS_MAC)
  for (auto&& extension : extensions)
    extension->Revoke();
#endif  // defined(OS_MAC)

  // Always called within the sandbox.
  if (success)
    instance->InitializeCdmModule();
}

void CdmService::CreateCdmFactory(
    mojo::PendingReceiver<mojom::CdmFactory> receiver,
    mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces) {
  // Ignore receiver if service has already stopped.
  if (!client_)
    return;

  cdm_factory_receivers_.AddReceiver(
      std::make_unique<CdmFactoryImpl>(client_.get(),
                                       std::move(frame_interfaces)),
      std::move(receiver));
}

}  // namespace media
