// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_AUDIO_SERVICE_AUDIO_PROCESSOR_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_AUDIO_SERVICE_AUDIO_PROCESSOR_PROXY_H_

#include <memory>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "media/base/audio_processing.h"
#include "media/webrtc/audio_processor_controls.h"
#include "third_party/blink/renderer/platform/mediastream/aec_dump_agent_impl.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/webrtc/api/media_stream_interface.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"
#include "third_party/webrtc/rtc_base/task_queue.h"

namespace blink {

class AecDumpAgentImpl;

// This class owns an object of webrtc::AudioProcessing which contains signal
// processing components like AGC, AEC and NS. It enables the components based
// on the getUserMedia constraints, processes the data and outputs it in a unit
// of 10 ms data chunk.
// TODO(https://crbug.com/879296): Add tests. Possibly the timer update rate
// calculation code should be encapsulated in a class.
class PLATFORM_EXPORT AudioServiceAudioProcessorProxy
    : public webrtc::AudioProcessorInterface,
      public AecDumpAgentImpl::Delegate {
 public:
  // All methods (including constructor and destructor) must be called on the
  // main thread except for GetStats.
  AudioServiceAudioProcessorProxy(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);

  // Stops the audio processor, no more AEC dump or render data after calling
  // this method.
  void Stop();

  // This method is called on the libjingle thread.
  AudioProcessorStatistics GetStats(bool has_remote_tracks) override;

  // AecDumpAgentImpl::Delegate implementation.
  // Called on the main render thread.
  void OnStartDump(base::File file) override;
  void OnStopDump() override;

  // Set the AudioProcessorControls which to proxy to. Must only be called once
  // and |controls| cannot be nullptr.
  void SetControls(media::AudioProcessorControls* controls);

 protected:
  ~AudioServiceAudioProcessorProxy() override;

 private:
  void RescheduleStatsUpdateTimer(base::TimeDelta new_interval);

  void RequestStats();
  void UpdateStats(const AudioProcessorStatistics& new_stats);
  AudioProcessorStatistics GetLatestStats();

  // This task runner is used to post tasks to the main thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner_;

  media::AudioProcessorControls* processor_controls_ = nullptr;
  base::RepeatingTimer stats_update_timer_;
  base::TimeTicks last_stats_request_time_;

  // |stats_lock_| protects both |target_stats_interval_| and |latest_stats_|.
  base::Lock stats_lock_;
  base::TimeDelta target_stats_interval_;
  AudioProcessorStatistics latest_stats_ = {};

  // Communication with browser for AEC dump.
  std::unique_ptr<AecDumpAgentImpl> aec_dump_agent_impl_;

  base::WeakPtrFactory<AudioServiceAudioProcessorProxy> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AudioServiceAudioProcessorProxy);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIASTREAM_AUDIO_SERVICE_AUDIO_PROCESSOR_PROXY_H_
