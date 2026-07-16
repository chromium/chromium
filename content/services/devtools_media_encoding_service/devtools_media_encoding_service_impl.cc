// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/devtools_media_encoding_service/devtools_media_encoding_service_impl.h"

namespace content {

DevToolsMediaEncodingServiceImpl::DevToolsMediaEncodingServiceImpl(
    mojo::PendingReceiver<
        devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
        receiver)
    : receiver_(this, std::move(receiver)) {}

DevToolsMediaEncodingServiceImpl::~DevToolsMediaEncodingServiceImpl() = default;

void DevToolsMediaEncodingServiceImpl::StartRecording(
    mojo::PendingRemote<devtools_media_encoding_service::mojom::
                            DevToolsMediaEncodingServiceClient> client,
    uint32_t max_width,
    uint32_t max_height,
    uint32_t frame_rate,
    bool has_audio) {
  if (client_.is_bound()) {
    receiver_.ReportBadMessage("Recording is already active");
    return;
  }
  client_.Bind(std::move(client));
}

void DevToolsMediaEncodingServiceImpl::RecordVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {}

void DevToolsMediaEncodingServiceImpl::RecordAudioBuffer(
    media::mojom::AudioBufferPtr buffer) {}

void DevToolsMediaEncodingServiceImpl::StopRecording() {
  if (client_.is_bound()) {
    client_->OnClosed();
    client_.reset();
  }
}

}  // namespace content
