// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_cdm_service.h"

#include <fuchsia/media/drm/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <string>

#include "base/bind.h"
#include "base/fuchsia/default_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_service_base.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "fuchsia/engine/switches.h"
#include "media/base/provision_fetcher.h"
#include "media/fuchsia/cdm/service/fuchsia_cdm_manager.h"
#include "media/fuchsia/mojom/fuchsia_cdm_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace {

class FuchsiaCdmProviderImpl
    : public content::FrameServiceBase<media::mojom::FuchsiaCdmProvider> {
 public:
  FuchsiaCdmProviderImpl(
      media::FuchsiaCdmManager* cdm_manager,
      media::CreateFetcherCB create_fetcher_cb,
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaCdmProvider> receiver);
  ~FuchsiaCdmProviderImpl() final;

  // media::mojom::FuchsiaCdmProvider implementation.
  void CreateCdmInterface(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) final;

 private:
  media::FuchsiaCdmManager* const cdm_manager_;
  const media::CreateFetcherCB create_fetcher_cb_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaCdmProviderImpl);
};

FuchsiaCdmProviderImpl::FuchsiaCdmProviderImpl(
    media::FuchsiaCdmManager* cdm_manager,
    media::CreateFetcherCB create_fetcher_cb,
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaCdmProvider> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      cdm_manager_(cdm_manager),
      create_fetcher_cb_(std::move(create_fetcher_cb)) {
  DCHECK(cdm_manager_);
}

FuchsiaCdmProviderImpl::~FuchsiaCdmProviderImpl() = default;

void FuchsiaCdmProviderImpl::CreateCdmInterface(
    const std::string& key_system,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  cdm_manager_->CreateAndProvision(key_system, origin(), create_fetcher_cb_,
                                   std::move(request));
}

void BindFuchsiaCdmProvider(
    media::FuchsiaCdmManager* cdm_manager,
    mojo::PendingReceiver<media::mojom::FuchsiaCdmProvider> receiver,
    content::RenderFrameHost* const frame_host) {
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(
          frame_host->GetProcess()->GetBrowserContext())
          ->GetURLLoaderFactoryForBrowserProcess();

  // The object will delete itself when connection to the frame is broken.
  new FuchsiaCdmProviderImpl(
      cdm_manager,
      base::BindRepeating(&content::CreateProvisionFetcher,
                          std::move(loader_factory)),
      frame_host, std::move(receiver));
}

class WidevineHandler : public media::FuchsiaCdmManager::KeySystemHandler {
 public:
  WidevineHandler() = default;
  ~WidevineHandler() override = default;

  void CreateCdm(
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) override {
    auto widevine = base::fuchsia::ComponentContextForCurrentProcess()
                        ->svc()
                        ->Connect<fuchsia::media::drm::Widevine>();
    widevine->CreateContentDecryptionModule(std::move(request));
  }

  fuchsia::media::drm::ProvisionerPtr CreateProvisioner() override {
    fuchsia::media::drm::ProvisionerPtr provisioner;

    auto widevine = base::fuchsia::ComponentContextForCurrentProcess()
                        ->svc()
                        ->Connect<fuchsia::media::drm::Widevine>();
    widevine->CreateProvisioner(provisioner.NewRequest());

    return provisioner;
  }
};

class PlayreadyHandler : public media::FuchsiaCdmManager::KeySystemHandler {
 public:
  PlayreadyHandler() = default;
  ~PlayreadyHandler() override = default;

  void CreateCdm(
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) override {
    auto playready = base::fuchsia::ComponentContextForCurrentProcess()
                         ->svc()
                         ->Connect<fuchsia::media::drm::PlayReady>();
    playready->CreateContentDecryptionModule(std::move(request));
  }

  fuchsia::media::drm::ProvisionerPtr CreateProvisioner() override {
    // Provisioning is not required for PlayReady.
    return fuchsia::media::drm::ProvisionerPtr();
  }
};

// Supported key systems:
std::unique_ptr<media::FuchsiaCdmManager> CreateCdmManager() {
  media::FuchsiaCdmManager::KeySystemHandlerMap handlers;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWidevine)) {
    handlers.emplace(kWidevineKeySystem, std::make_unique<WidevineHandler>());
  }

  std::string playready_key_system =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kPlayreadyKeySystem);
  if (!playready_key_system.empty()) {
    handlers.emplace(playready_key_system,
                     std::make_unique<PlayreadyHandler>());
  }

  return std::make_unique<media::FuchsiaCdmManager>(std::move(handlers));
}

}  // namespace

WebEngineCdmService::WebEngineCdmService(
    service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
        registry)
    : cdm_manager_(CreateCdmManager()), registry_(registry) {
  DCHECK(cdm_manager_);
  DCHECK(registry_);
  registry_->AddInterface(
      base::BindRepeating(&BindFuchsiaCdmProvider, cdm_manager_.get()));
}

WebEngineCdmService::~WebEngineCdmService() {
  registry_->RemoveInterface<media::mojom::FuchsiaCdmProvider>();
}
