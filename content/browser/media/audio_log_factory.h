// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_LOG_FACTORY_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_LOG_FACTORY_H_

#include "media/mojo/mojom/audio_logging.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

class AudioLogFactory : public media::mojom::AudioLogFactory {
 public:
  AudioLogFactory();

  AudioLogFactory(const AudioLogFactory&) = delete;
  AudioLogFactory& operator=(const AudioLogFactory&) = delete;

  ~AudioLogFactory() override;

  // media::mojom::AudioLogFactory implementation.
  void CreateAudioLog(media::mojom::AudioLogComponent component,
                      int32_t component_id,
                      mojo::PendingReceiver<media::mojom::AudioLog>
                          audio_log_receiver) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_LOG_FACTORY_H_
