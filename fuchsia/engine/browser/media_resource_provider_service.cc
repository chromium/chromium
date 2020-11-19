// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/media_resource_provider_service.h"

#include <lib/fidl/cpp/interface_handle.h>
#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/fuchsia/process_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_service_base.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/switches.h"
#include "media/base/media_switches.h"
#include "media/base/provision_fetcher.h"
#include "media/fuchsia/cdm/service/fuchsia_cdm_manager.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace {

class MediaResourceProviderImpl
    : public content::FrameServiceBase<
          media::mojom::FuchsiaMediaResourceProvider> {
 public:
  MediaResourceProviderImpl(
      media::FuchsiaCdmManager* cdm_manager,
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
          receiver);
  ~MediaResourceProviderImpl() final;

  MediaResourceProviderImpl(const MediaResourceProviderImpl&) = delete;
  MediaResourceProviderImpl& operator=(const MediaResourceProviderImpl&) =
      delete;

  // media::mojom::FuchsiaMediaResourceProvider implementation.
  void CreateCdm(
      const std::string& key_system,
      fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
          request) final;
  void CreateAudioConsumer(
      fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request) final;
  void CreateAudioCapturer(
      fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request) final;

 private:
  media::FuchsiaCdmManager* const cdm_manager_;
};

MediaResourceProviderImpl::MediaResourceProviderImpl(
    media::FuchsiaCdmManager* cdm_manager,
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      cdm_manager_(cdm_manager) {
  DCHECK(cdm_manager_);
}

MediaResourceProviderImpl::~MediaResourceProviderImpl() = default;

void MediaResourceProviderImpl::CreateCdm(
    const std::string& key_system,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(
          render_frame_host()->GetProcess()->GetBrowserContext())
          ->GetURLLoaderFactoryForBrowserProcess();
  media::CreateFetcherCB create_fetcher_cb = base::BindRepeating(
      &content::CreateProvisionFetcher, std::move(loader_factory));
  cdm_manager_->CreateAndProvision(
      key_system, origin(), std::move(create_fetcher_cb), std::move(request));
}

void MediaResourceProviderImpl::CreateAudioConsumer(
    fidl::InterfaceRequest<fuchsia::media::AudioConsumer> request) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioOutput)) {
    LOG(WARNING)
        << "Could not create AudioConsumer because audio output feature flag "
           "was not enabled.";
    return;
  }

  auto factory = base::ComponentContextForProcess()
                     ->svc()
                     ->Connect<fuchsia::media::SessionAudioConsumerFactory>();
  factory->CreateAudioConsumer(
      FrameImpl::FromRenderFrameHost(render_frame_host())->media_session_id(),
      std::move(request));
}

void MediaResourceProviderImpl::CreateAudioCapturer(
    fidl::InterfaceRequest<fuchsia::media::AudioCapturer> request) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAudioInput)) {
    LOG(WARNING)
        << "Could not create AudioCapturer because audio input feature flag "
           "was not enabled.";
    return;
  }

  if (FrameImpl::FromRenderFrameHost(render_frame_host())
          ->permission_controller()
          ->GetPermissionState(content::PermissionType::AUDIO_CAPTURE,
                               origin()) !=
      blink::mojom::PermissionStatus::GRANTED) {
    DLOG(WARNING)
        << "Received CreateAudioCapturer request from an origin that doesn't "
           "have AUDIO_CAPTURE permission.";
    return;
  }

  auto factory = base::ComponentContextForProcess()
                     ->svc()
                     ->Connect<fuchsia::media::Audio>();
  factory->CreateAudioCapturer(std::move(request), /*loopback=*/false);
}

template <typename KeySystemInterface>
fidl::InterfaceHandle<fuchsia::media::drm::KeySystem> ConnectToKeySystem() {
  static_assert(
      (std::is_same<KeySystemInterface, fuchsia::media::drm::Widevine>::value ||
       std::is_same<KeySystemInterface, fuchsia::media::drm::PlayReady>::value),
      "KeySystemInterface must be either fuchsia::media::drm::Widevine or "
      "fuchsia::media::drm::PlayReady");

  // TODO(fxbug.dev/13674): Once the key system specific protocols are turned
  // into services, we should not need to manually force the key system specific
  // interface into the KeySystem interface.
  fidl::InterfaceHandle<fuchsia::media::drm::KeySystem> key_system;
  base::ComponentContextForProcess()->svc()->Connect(key_system.NewRequest(),
                                                     KeySystemInterface::Name_);
  return key_system;
}

std::unique_ptr<media::FuchsiaCdmManager> CreateCdmManager() {
  media::FuchsiaCdmManager::CreateKeySystemCallbackMap
      create_key_system_callbacks;

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableWidevine)) {
    create_key_system_callbacks.emplace(
        kWidevineKeySystem,
        base::BindRepeating(
            &ConnectToKeySystem<fuchsia::media::drm::Widevine>));
  }

  std::string playready_key_system =
      command_line->GetSwitchValueASCII(switches::kPlayreadyKeySystem);
  if (!playready_key_system.empty()) {
    create_key_system_callbacks.emplace(
        playready_key_system,
        base::BindRepeating(
            &ConnectToKeySystem<fuchsia::media::drm::PlayReady>));
  }

  std::string cdm_data_directory =
      command_line->GetSwitchValueASCII(switches::kCdmDataDirectory);

  base::Optional<uint64_t> cdm_data_quota_bytes;
  if (command_line->HasSwitch(switches::kCdmDataQuotaBytes)) {
    uint64_t value = 0;
    CHECK(base::StringToUint64(
        command_line->GetSwitchValueASCII(switches::kCdmDataQuotaBytes),
        &value));
    cdm_data_quota_bytes = value;
  }

  return std::make_unique<media::FuchsiaCdmManager>(
      std::move(create_key_system_callbacks),
      base::FilePath(cdm_data_directory), cdm_data_quota_bytes);
}

}  // namespace

MediaResourceProviderService::MediaResourceProviderService()
    : cdm_manager_(CreateCdmManager()) {}

MediaResourceProviderService::~MediaResourceProviderService() = default;

void MediaResourceProviderService::Bind(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
        receiver) {
  // The object will delete itself when connection to the frame is broken.
  new MediaResourceProviderImpl(cdm_manager_.get(), frame_host,
                                std::move(receiver));
}
