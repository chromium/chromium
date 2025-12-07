// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_PULSE_LOOPBACK_MANAGER_H_
#define MEDIA_AUDIO_PULSE_PULSE_LOOPBACK_MANAGER_H_

#include <pulse/pulseaudio.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "pulse_loopback.h"

#include <memory>
#include <string>

namespace media {

// This class is required when performing system audio loopback capture. It
// manages PulseLoopbackAudioStreams, which are kind of PulseAudioInputStream
// which is guaranteed to always deliver system audio. There must only be one
// instance of this class associated with a particular pa_context.
class PulseLoopbackManager {
 public:
  using ReleaseStreamCallback =
      base::RepeatingCallback<void(AudioInputStream*)>;

  static std::unique_ptr<PulseLoopbackManager> Create(
      ReleaseStreamCallback release_stream_callback,
      pa_context* context,
      pa_threaded_mainloop* mainloop);

  ~PulseLoopbackManager();

  // Creates a new loopback stream, with properties the same as
  // AudioDeviceDescription::kLoopbackInputDeviceId.
  PulseLoopbackAudioStream* MakeLoopbackStream(
      const AudioParameters& params,
      AudioManager::LogCallback log_callback,
      bool should_mute_system_audio);

  // Removes a loopback stream.
  void RemoveStream(AudioInputStream* stream);

  // Current default sink monitor name.
  const std::string& GetLoopbackSourceName();

 private:
  // Called by PulseAudio when an event occurs. Runs on a PulseAudio thread.
  // pa_context_subscribe_cb_t
  static void EventCallback(pa_context* context,
                            pa_subscription_event_type_t type,
                            uint32_t index,
                            void* user_data);

  PulseLoopbackManager(const std::string& default_monitor_name,
                       ReleaseStreamCallback release_stream_callback,
                       pa_context* context,
                       pa_threaded_mainloop* mainloop);

  // Called when either the default sink or source changes.
  void OnServerChangeEvent();

  // Name of the monitor associated with the current default sink.
  std::string default_monitor_name_;

  // Passed to newly created streams to be called when they close.
  ReleaseStreamCallback release_stream_callback_;

  // Global PulseAudio context and mainloop.
  const raw_ptr<pa_context> context_;
  const raw_ptr<pa_threaded_mainloop> mainloop_;

  bool has_muting_loopback_ = false;

  // Currently open loopback streams.
  std::vector<raw_ptr<PulseLoopbackAudioStream>> streams_;

  THREAD_CHECKER(thread_checker_);
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtr<PulseLoopbackManager> weak_this_;
  base::WeakPtrFactory<PulseLoopbackManager> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_PULSE_LOOPBACK_MANAGER_H_
