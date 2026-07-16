// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_DEVTOOLS_MEDIA_ENCODING_SERVICE_DEVTOOLS_MEDIA_ENCODING_SERVICE_IMPL_H_
#define CONTENT_SERVICES_DEVTOOLS_MEDIA_ENCODING_SERVICE_DEVTOOLS_MEDIA_ENCODING_SERVICE_IMPL_H_

#include "content/services/devtools_media_encoding_service/public/mojom/devtools_media_encoding_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "content/common/content_export.h"

namespace content {

class CONTENT_EXPORT DevToolsMediaEncodingServiceImpl : public devtools_media_encoding_service::
                                            mojom::DevToolsMediaEncodingService {
 public:
  explicit DevToolsMediaEncodingServiceImpl(
      mojo::PendingReceiver<
          devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
          receiver);
  DevToolsMediaEncodingServiceImpl(const DevToolsMediaEncodingServiceImpl&) =
      delete;
  DevToolsMediaEncodingServiceImpl& operator=(
      const DevToolsMediaEncodingServiceImpl&) = delete;
  ~DevToolsMediaEncodingServiceImpl() override;

  // devtools_media_encoding_service::mojom::DevToolsMediaEncodingService:
  void StartRecording(
      mojo::PendingRemote<devtools_media_encoding_service::mojom::
                              DevToolsMediaEncodingServiceClient> client,
      uint32_t max_width,
      uint32_t max_height,
      uint32_t frame_rate,
      bool has_audio) override;
  void RecordVideoFrame(const scoped_refptr<media::VideoFrame>& frame) override;
  void RecordAudioBuffer(media::mojom::AudioBufferPtr buffer) override;
  void StopRecording() override;

 private:
  mojo::Receiver<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingService>
      receiver_;
  mojo::Remote<
      devtools_media_encoding_service::mojom::DevToolsMediaEncodingServiceClient>
      client_;
};

}  // namespace content

#endif  // CONTENT_SERVICES_DEVTOOLS_MEDIA_ENCODING_SERVICE_DEVTOOLS_MEDIA_ENCODING_SERVICE_IMPL_H_
