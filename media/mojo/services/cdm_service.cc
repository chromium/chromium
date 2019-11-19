// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/cdm_service.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/cdm_factory.h"
#include "media/cdm/cdm_module.h"
#include "media/media_buildflags.h"
#include "media/mojo/services/mojo_cdm_service.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/service_manager/public/cpp/connector.h"

#if defined(OS_MACOSX)
#include <vector>
#include "sandbox/mac/seatbelt_extension.h"
#endif  // defined(OS_MACOSX)

namespace media {

namespace {

using service_manager::ServiceKeepaliveRef;

constexpr base::TimeDelta kKeepaliveIdleTimeout =
    base::TimeDelta::FromSeconds(5);

// Implementation of mojom::CdmFactory that creates and hosts MojoCdmServices
// which then host CDMs created by the media::CdmFactory provided by the
// CdmService::Client.
//
// Lifetime Note:
// 1. CdmFactoryImpl instances are owned by a DeferredDestroyStrongBindingSet
//    directly, which is owned by CdmService.
// 2. Note that CdmFactoryImpl also holds a ServiceKeepaliveRef tied to the
//    CdmService.
// 3. CdmFactoryImpl is destroyed in any of the following two cases:
//   - CdmService is destroyed. Because of (2) this should not happen except for
//     during browser shutdown, when the Cdservice could be destroyed directly,
//     ignoring any outstanding ServiceKeepaliveRefs.
//   - mojo::CdmFactory connection error happens, AND CdmFactoryImpl doesn't own
//     any CDMs (|cdm_receivers_| is empty). This is to prevent destroying the
//     CDMs too early (e.g. during page navigation) which could cause errors
//     (session closed) on the client side. See https://crbug.com/821171 for
//     details.
class CdmFactoryImpl : public DeferredDestroy<mojom::CdmFactory> {
 public:
  CdmFactoryImpl(
      CdmService::Client* client,
      mojo::PendingRemote<service_manager::mojom::InterfaceProvider> interfaces,
      std::unique_ptr<ServiceKeepaliveRef> keepalive_ref)
      : client_(client),
        interfaces_(std::move(interfaces)),
        keepalive_ref_(std::move(keepalive_ref)) {
    DVLOG(1) << __func__;

    // base::Unretained is safe because |cdm_receivers_| is owned by |this|. If
    // |this| is destructed, |cdm_receivers_| will be destructed as well and the
    // error handler should never be called.
    cdm_receivers_.set_disconnect_handler(base::BindRepeating(
        &CdmFactoryImpl::OnBindingConnectionError, base::Unretained(this)));
  }

  ~CdmFactoryImpl() final { DVLOG(1) << __func__; }

  // mojom::CdmFactory implementation.
  void CreateCdm(
      const std::string& key_system,
      mojo::PendingReceiver<mojom::ContentDecryptionModule> receiver) final {
    DVLOG(2) << __func__;

    auto* cdm_factory = GetCdmFactory();
    if (!cdm_factory)
      return;

    cdm_receivers_.Add(
        std::make_unique<MojoCdmService>(cdm_factory, &cdm_service_context_),
        std::move(receiver));
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

  void OnBindingConnectionError() {
    if (destroy_cb_ && cdm_receivers_.empty())
      std::move(destroy_cb_).Run();
  }

  // Must be declared before the receivers below because the bound objects might
  // take a raw pointer of |cdm_service_context_| and assume it's always
  // available.
  MojoCdmServiceContext cdm_service_context_;

  CdmService::Client* client_;
  mojo::Remote<service_manager::mojom::InterfaceProvider> interfaces_;
  mojo::UniqueReceiverSet<mojom::ContentDecryptionModule> cdm_receivers_;
  std::unique_ptr<ServiceKeepaliveRef> keepalive_ref_;
  std::unique_ptr<media::CdmFactory> cdm_factory_;
  base::OnceClosure destroy_cb_;

  DISALLOW_COPY_AND_ASSIGN(CdmFactoryImpl);
};

}  // namespace

CdmService::CdmService(
    std::unique_ptr<Client> client,
    mojo::PendingReceiver<service_manager::mojom::Service> receiver)
    : service_binding_(this, std::move(receiver)),
      keepalive_(std::make_unique<service_manager::ServiceKeepalive>(
          &service_binding_,
          kKeepaliveIdleTimeout)),
      client_(std::move(client)) {
  DVLOG(1) << __func__;
  DCHECK(client_);
  registry_.AddInterface<mojom::CdmService>(
      base::BindRepeating(&CdmService::Create, base::Unretained(this)));
}

CdmService::~CdmService() {
  DVLOG(1) << __func__;
}

void CdmService::SetServiceReleaseDelayForTesting(base::TimeDelta delay) {
  DCHECK(keepalive_->HasNoRefs());
  keepalive_ = std::make_unique<service_manager::ServiceKeepalive>(
      &service_binding_, delay);
}

void CdmService::OnStart() {
  DVLOG(1) << __func__;
}

void CdmService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DVLOG(1) << __func__ << ": interface_name = " << interface_name;

  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

void CdmService::OnDisconnected() {
  cdm_factory_bindings_.CloseAllBindings();
  client_.reset();
  Terminate();
}

void CdmService::Create(mojo::PendingReceiver<mojom::CdmService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

#if defined(OS_MACOSX)
void CdmService::LoadCdm(
    const base::FilePath& cdm_path,
    mojom::SeatbeltExtensionTokenProviderPtr token_provider) {
#else
void CdmService::LoadCdm(const base::FilePath& cdm_path) {
#endif  // defined(OS_MACOSX)
  DVLOG(1) << __func__ << ": cdm_path = " << cdm_path.value();

  // Ignore request if service has already stopped.
  if (!client_)
    return;

  CdmModule* instance = CdmModule::GetInstance();
  if (instance->was_initialize_called()) {
    DCHECK_EQ(cdm_path, instance->GetCdmPath());
    return;
  }

#if defined(OS_MACOSX)
  std::vector<std::unique_ptr<sandbox::SeatbeltExtension>> extensions;

  if (token_provider) {
    std::vector<sandbox::SeatbeltExtensionToken> tokens;
    CHECK(token_provider->GetTokens(&tokens));

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
#endif  // defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  std::vector<CdmHostFilePath> cdm_host_file_paths;
  client_->AddCdmHostFilePaths(&cdm_host_file_paths);
  bool success = instance->Initialize(cdm_path, cdm_host_file_paths);
#else
  bool success = instance->Initialize(cdm_path);
#endif  // BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)

  // This may trigger the sandbox to be sealed.
  client_->EnsureSandboxed();

#if defined(OS_MACOSX)
  for (auto&& extension : extensions)
    extension->Revoke();
#endif  // defined(OS_MACOSX)

  // Always called within the sandbox.
  if (success)
    instance->InitializeCdmModule();
}

void CdmService::CreateCdmFactory(
    mojo::PendingReceiver<mojom::CdmFactory> receiver,
    mojo::PendingRemote<service_manager::mojom::InterfaceProvider>
        host_interfaces) {
  // Ignore receiver if service has already stopped.
  if (!client_)
    return;

  cdm_factory_bindings_.AddBinding(
      std::make_unique<CdmFactoryImpl>(
          client_.get(), std::move(host_interfaces), keepalive_->CreateRef()),
      std::move(receiver));
}

}  // namespace media
