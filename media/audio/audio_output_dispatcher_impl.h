// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioOutputDispatcherImpl is an implementation of AudioOutputDispatcher.
//
// To avoid opening and closing audio devices more frequently than necessary,
// each dispatcher has a pool of inactive physical streams. A stream is closed
// only if it hasn't been used for a certain period of time (specified via the
// constructor).
//

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_IMPL_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_output_dispatcher.h"
#include "media/base/audio_parameters.h"

namespace media {
class AudioLog;

class MEDIA_EXPORT AudioOutputDispatcherImpl : public AudioOutputDispatcher {
 public:
  // |close_delay| specifies delay after the stream is idle until the audio
  // device is closed.
  AudioOutputDispatcherImpl(AudioManager* audio_manager,
                            const AudioParameters& params,
                            const std::string& output_device_id,
                            base::TimeDelta close_delay);
  ~AudioOutputDispatcherImpl() override;

  // AudioOutputDispatcher implementation.
  AudioOutputProxy* CreateStreamProxy() override;
  bool OpenStream() override;
  bool StartStream(AudioOutputStream::AudioSourceCallback* callback,
                   AudioOutputProxy* stream_proxy) override;
  void StopStream(AudioOutputProxy* stream_proxy) override;
  void StreamVolumeSet(AudioOutputProxy* stream_proxy, double volume) override;
  void CloseStream(AudioOutputProxy* stream_proxy) override;
  void FlushStream(AudioOutputProxy* stream_proxy) override;

  // Returns true if there are any open AudioOutputProxy objects.
  bool HasOutputProxies() const;

  // Closes all |idle_streams_|.
  void CloseAllIdleStreams();

 private:
  // Creates a new physical output stream, opens it and pushes to
  // |idle_streams_|.  Returns false if the stream couldn't be created or
  // opened.
  bool CreateAndOpenStream();

  // Similar to CloseAllIdleStreams(), but keeps |keep_alive| streams alive.
  void CloseIdleStreams(size_t keep_alive);

  void StopPhysicalStream(AudioOutputStream* stream);

  // Output parameters.
  const AudioParameters params_;

  // Output device id.
  const std::string device_id_;

  size_t idle_proxies_;
  std::vector<AudioOutputStream*> idle_streams_;

  // When streams are stopped they're added to |idle_streams_|, if no stream is
  // reused before |close_delay_| elapses |close_timer_| will run
  // CloseIdleStreams().
  base::DelayTimer close_timer_;

  typedef base::flat_map<AudioOutputProxy*, AudioOutputStream*> AudioStreamMap;
  AudioStreamMap proxy_to_physical_map_;

  using AudioLogMap =
      base::flat_map<AudioOutputStream*, std::unique_ptr<media::AudioLog>>;
  AudioLogMap audio_logs_;
  int audio_stream_id_;

  base::WeakPtrFactory<AudioOutputDispatcherImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AudioOutputDispatcherImpl);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_IMPL_H_
