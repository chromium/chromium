// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_OUTPUT_DEVICE_H_
#define SERVICES_AUDIO_PUBLIC_CPP_OUTPUT_DEVICE_H_

#include <memory>
#include <string>

#include "media/base/audio_renderer_sink.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/audio/public/mojom/audio_processing.mojom.h"
#include "services/audio/public/mojom/stream_factory.mojom.h"

namespace media {
class AudioDeviceThread;
class AudioOutputDeviceThreadCallback;
}  // namespace media

namespace audio {

class OutputDevice {
 public:
  // media::AudioRendererSink::RenderCallback must outlive |this|.
  OutputDevice(mojo::PendingRemote<mojom::StreamFactory> stream_factory,
               const media::AudioParameters& params,
               media::AudioRendererSink::RenderCallback* callback,
               const std::string& device_id);

  // Blocking call; see base/threading/thread_restrictions.h.
  ~OutputDevice();

  void Play();
  void Pause();
  void SetVolume(double volume);

 private:
  void StreamCreated(media::mojom::ReadWriteAudioDataPipePtr data_pipe);
  void OnConnectionError();
  void CleanUp();  // Blocking call.

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<media::AudioOutputDeviceThreadCallback> audio_callback_;
  std::unique_ptr<media::AudioDeviceThread> audio_thread_;
  media::AudioParameters audio_parameters_;
  media::AudioRendererSink::RenderCallback* render_callback_;
  mojo::Remote<media::mojom::AudioOutputStream> stream_;
  mojo::Remote<audio::mojom::StreamFactory> stream_factory_;

  base::WeakPtrFactory<OutputDevice> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OutputDevice);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_OUTPUT_DEVICE_H_
