// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/web_engine_media_resource_provider_impl.h"

#include <lib/fidl/cpp/interface_handle.h>
#include <lib/sys/cpp/component_context.h>

#include "base/command_line.h"
#include "base/fuchsia/process_context.h"
#include "base/functional/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/document_service.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "media/base/media_switches.h"

void WebEngineMediaResourceProviderImpl::Bind(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<mojom::WebEngineMediaResourceProvider> receiver) {
  CHECK(frame_host);
  // The object will delete itself when connection to the frame is broken.
  new WebEngineMediaResourceProviderImpl(*frame_host, std::move(receiver));
}

WebEngineMediaResourceProviderImpl::WebEngineMediaResourceProviderImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::WebEngineMediaResourceProvider> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

WebEngineMediaResourceProviderImpl::~WebEngineMediaResourceProviderImpl() =
    default;

void WebEngineMediaResourceProviderImpl::ShouldUseAudioConsumer(
    ShouldUseAudioConsumerCallback callback) {
  auto* frame_impl = FrameImpl::FromRenderFrameHost(&render_frame_host());
  DCHECK(frame_impl);
  std::move(callback).Run(
      frame_impl->media_settings().has_audio_consumer_session_id());
}

void WebEngineMediaResourceProviderImpl::CreateAudioConsumer(
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
  auto* frame_impl = FrameImpl::FromRenderFrameHost(&render_frame_host());
  DCHECK(frame_impl);

  if (!frame_impl->media_settings().has_audio_consumer_session_id()) {
    LOG(WARNING) << "Renderer tried creating AudioConsumer for a Frame without "
                    "media_session_id().";
    return;
  }

  factory->CreateAudioConsumer(
      frame_impl->media_settings().audio_consumer_session_id(),
      std::move(request));
}
