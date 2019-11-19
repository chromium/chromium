// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_AUDIO_LOOPBACK_STREAM_CREATOR_H_
#define CONTENT_PUBLIC_BROWSER_AUDIO_LOOPBACK_STREAM_CREATOR_H_

#include "base/callback.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {
class AudioParameters;
}

namespace content {

class WebContents;

// This interface is used by the embedder to ask the Audio Service to create a
// loopback stream that either captures audio from a tab or the system-wide
// loopback.
// Note: Use this to request loopback audio for a privileged embedder feature,
// and not for consumption by a renderer. For renderers, use the
// mojom::RendererAudioInputStreamFactory interface instead.
class CONTENT_EXPORT AudioLoopbackStreamCreator {
 public:
  virtual ~AudioLoopbackStreamCreator();

  // The callback that is called when the requested stream is created.
  using StreamCreatedCallback = base::RepeatingCallback<void(
      mojo::PendingRemote<media::mojom::AudioInputStream> stream,
      mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
          client_receiver,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe)>;

  // Creates an InProcessAudioLoopbackStreamCreator that handles creating audio
  // loopback stream through the Audio Service.
  static std::unique_ptr<AudioLoopbackStreamCreator>
  CreateInProcessAudioLoopbackStreamCreator();

  // Creates a loopback stream that captures the audio from |loopback_source|,
  // or the default system playback if |loopback_source| is null. Local output
  // of the source/system audio is muted during capturing.
  virtual void CreateLoopbackStream(WebContents* loopback_source,
                                    const media::AudioParameters& params,
                                    uint32_t total_segments,
                                    const StreamCreatedCallback& callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_AUDIO_LOOPBACK_STREAM_CREATOR_H_
