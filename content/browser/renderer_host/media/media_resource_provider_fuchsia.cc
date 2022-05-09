// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_resource_provider_fuchsia.h"

#include <fuchsia/mediacodec/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/fuchsia/process_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/provision_fetcher_factory.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "media/base/media_switches.h"
#include "media/base/provision_fetcher.h"
#include "media/fuchsia/cdm/service/fuchsia_cdm_manager.h"

namespace content {

namespace {

absl::optional<std::string> GetMimeTypeForVideoCodec(media::VideoCodec codec) {
  switch (codec) {
    case media::VideoCodec::kH264:
      return "video/h264";
    case media::VideoCodec::kVP8:
      return "video/vp8";
    case media::VideoCodec::kVP9:
      return "video/vp9";
    case media::VideoCodec::kHEVC:
      return "video/hevc";
    case media::VideoCodec::kAV1:
      return "video/av1";

    case media::VideoCodec::kVC1:
    case media::VideoCodec::kMPEG2:
    case media::VideoCodec::kMPEG4:
    case media::VideoCodec::kTheora:
    case media::VideoCodec::kDolbyVision:
      return absl::nullopt;

    case media::VideoCodec::kUnknown:
      break;
  }

  NOTREACHED();
  return absl::nullopt;
}

}  // namespace

// static
void MediaResourceProviderFuchsia::Bind(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider>
        receiver) {
  // The object will delete itself when connection to the frame is broken.
  new MediaResourceProviderFuchsia(frame_host, std::move(receiver));
}

MediaResourceProviderFuchsia::MediaResourceProviderFuchsia(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::FuchsiaMediaResourceProvider> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}
MediaResourceProviderFuchsia::~MediaResourceProviderFuchsia() = default;

void MediaResourceProviderFuchsia::CreateCdm(
    const std::string& key_system,
    fidl::InterfaceRequest<fuchsia::media::drm::ContentDecryptionModule>
        request) {
  auto* cdm_manager = media::FuchsiaCdmManager::GetInstance();
  if (!cdm_manager) {
    DLOG(WARNING) << "FuchsiaCdmManager hasn't been initialized. Dropping "
                     "CreateCdm() request.";
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      render_frame_host()
          ->GetStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  media::CreateFetcherCB create_fetcher_cb = base::BindRepeating(
      &content::CreateProvisionFetcher, std::move(url_loader_factory));
  cdm_manager->CreateAndProvision(
      key_system, origin(), std::move(create_fetcher_cb), std::move(request));
}

void MediaResourceProviderFuchsia::CreateVideoDecoder(
    media::VideoCodec codec,
    bool secure_mode,
    fidl::InterfaceRequest<fuchsia::media::StreamProcessor>
        stream_processor_request) {
  fuchsia::mediacodec::CreateDecoder_Params decoder_params;

  // Set format details ordinal to 0. Decoder doesn't change the format, so
  // the value doesn't matter.
  decoder_params.mutable_input_details()->set_format_details_version_ordinal(0);

  auto mime_type = GetMimeTypeForVideoCodec(codec);
  if (!mime_type) {
    // Drop `stream_processor_request` if the codec is not supported.
    return;
  }
  decoder_params.mutable_input_details()->set_mime_type(mime_type.value());

  if (secure_mode) {
    decoder_params.set_secure_input_mode(
        fuchsia::mediacodec::SecureMemoryMode::ON);
  }

  if (secure_mode || base::CommandLine::ForCurrentProcess()->HasSwitch(
                         switches::kForceProtectedVideoOutputBuffers)) {
    decoder_params.set_secure_output_mode(
        fuchsia::mediacodec::SecureMemoryMode::ON);
  }

  // Video demuxers return each video frame in a separate packet. This field
  // must be set to get frame timestamps on the decoder output.
  decoder_params.set_promise_separate_access_units_on_input(true);

  // We use `fuchsia.mediacodec` only for hardware decoders. Renderer will
  // handle software decoding if hardware decoder is not available.
  decoder_params.set_require_hw(true);

  auto decoder_factory = base::ComponentContextForProcess()
                             ->svc()
                             ->Connect<fuchsia::mediacodec::CodecFactory>();
  decoder_factory->CreateDecoder(std::move(decoder_params),
                                 std::move(stream_processor_request));
}

}  // namespace content
