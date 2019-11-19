// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_FUCHSIA_AUDIO_OUTPUT_STREAM_FUCHSIA_H_
#define MEDIA_AUDIO_FUCHSIA_AUDIO_OUTPUT_STREAM_FUCHSIA_H_

#include <fuchsia/media/cpp/fidl.h>

#include "base/memory/shared_memory_mapping.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerFuchsia;

class AudioOutputStreamFuchsia : public AudioOutputStream {
 public:
  // Caller must ensure that manager outlives the stream.
  AudioOutputStreamFuchsia(AudioManagerFuchsia* manager,
                           const AudioParameters& parameters);

  // AudioOutputStream interface.
  bool Open() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void Flush() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void Close() override;

 private:
  ~AudioOutputStreamFuchsia() override;

  // Returns minimum |payload_buffer_| size for the current |min_lead_time_|.
  size_t GetMinBufferSize();

  // Allocates and maps |payload_buffer_|.
  bool InitializePayloadBuffer();

  base::TimeTicks GetCurrentStreamTime();

  // Event handler for |audio_out_|.
  void OnMinLeadTimeChanged(int64_t min_lead_time);

  // Error handler for |audio_out_|.
  void OnRendererError(zx_status_t status);

  // Resets internal state and reports an error to |callback_|.
  void ReportError();

  // Requests data from AudioSourceCallback, passes it to the mixer and
  // schedules |timer_| for the next call.
  void PumpSamples();

  // Schedules |timer_| to call PumpSamples() when appropriate for the next
  // packet.
  void SchedulePumpSamples(base::TimeTicks now);

  AudioManagerFuchsia* manager_;
  AudioParameters parameters_;

  fuchsia::media::AudioRendererPtr audio_renderer_;

  // |audio_bus_| is used only in PumpSamples(). It is kept here to avoid
  // reallocating the memory every time.
  std::unique_ptr<AudioBus> audio_bus_;

  base::WritableSharedMemoryMapping payload_buffer_;
  size_t payload_buffer_pos_ = 0;

  AudioSourceCallback* callback_ = nullptr;

  double volume_ = 1.0;

  base::TimeTicks reference_time_;

  int64_t stream_position_samples_;

  // Current min lead time for the stream. This value is not set until the first
  // AudioRenderer::OnMinLeadTimeChanged event.
  base::Optional<base::TimeDelta> min_lead_time_;

  // Timer that's scheduled to call PumpSamples().
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(AudioOutputStreamFuchsia);
};

}  // namespace media

#endif  // MEDIA_AUDIO_FUCHSIA_AUDIO_OUTPUT_STREAM_FUCHSIA_H_
