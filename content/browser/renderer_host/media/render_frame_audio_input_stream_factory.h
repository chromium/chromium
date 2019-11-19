// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_INPUT_STREAM_FACTORY_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_INPUT_STREAM_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/common/media/renderer_audio_input_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class MediaStreamManager;
class RenderFrameHost;

// Handles a RendererAudioInputStreamFactory pending receiver for a render frame
// host. Should be constructed and destructed on the UI thread, but will process
// mojo messages on the IO thread.
// This class relates to ForwardingAudioStreamFactory the same way as
// RenderFrameAudioOutputStreamFactory, and a class diagram can be found in
// render_frame_audio_output_stream_factory.h
class CONTENT_EXPORT RenderFrameAudioInputStreamFactory final {
 public:
  RenderFrameAudioInputStreamFactory(
      mojo::PendingReceiver<mojom::RendererAudioInputStreamFactory> receiver,
      MediaStreamManager* media_stream_manager,
      RenderFrameHost* render_frame_host);

  ~RenderFrameAudioInputStreamFactory();

 private:
  class Core;
  std::unique_ptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameAudioInputStreamFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_INPUT_STREAM_FACTORY_H_
