// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_PULSE_PULSE_UTIL_H_
#define MEDIA_AUDIO_PULSE_PULSE_UTIL_H_

#include <pulse/pulseaudio.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "media/audio/audio_device_name.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

namespace media {

class AudioParameters;

namespace pulse {

enum class RequestType : int8_t { INPUT, OUTPUT };

// A helper class that acquires pa_threaded_mainloop_lock() while in scope.
class AutoPulseLock {
 public:
  explicit AutoPulseLock(pa_threaded_mainloop* pa_mainloop)
      : pa_mainloop_(pa_mainloop) {
    pa_threaded_mainloop_lock(pa_mainloop_);
  }

  AutoPulseLock(const AutoPulseLock&) = delete;
  AutoPulseLock& operator=(const AutoPulseLock&) = delete;

  ~AutoPulseLock() {
    pa_threaded_mainloop_unlock(pa_mainloop_);
  }

 private:
  raw_ptr<pa_threaded_mainloop> pa_mainloop_;
};

bool MEDIA_EXPORT InitPulse(pa_threaded_mainloop** mainloop,
                            pa_context** context);
void DestroyPulse(pa_threaded_mainloop* mainloop, pa_context* context);

// Triggers pa_threaded_mainloop_signal() to avoid deadlocks.
void StreamSuccessCallback(pa_stream* s, int error, void* mainloop);
void ContextSuccessCallback(pa_context* context, int success, void* mainloop);
void ContextStateCallback(pa_context* context, void* mainloop);

// Mutes all audio output sinks except for a specified sink.
void MuteAllSinksExcept(pa_threaded_mainloop* mainloop,
                        pa_context* context,
                        const std::string& exclude_sink_name);
// Unmutes all audio output sinks in the system.
void UnmuteAllSinks(pa_threaded_mainloop* mainloop, pa_context* context);

pa_channel_map ChannelLayoutToPAChannelMap(ChannelLayout channel_layout);

// Blocks until pa_operation completes. If |optional_context| and/or
// |optional_stream| are provided, the method will cancel |operation| and return
// false if either the context or stream enter a bad state while waiting.
bool WaitForOperationCompletion(pa_threaded_mainloop* mainloop,
                                pa_operation* operation,
                                pa_context* optional_context = nullptr,
                                pa_stream* optional_stream = nullptr);

base::TimeDelta GetHardwareLatency(pa_stream* stream);

constexpr SampleFormat kInputSampleFormat = kSampleFormatS16;

// Create a recording stream for the threaded mainloop, return true if success,
// otherwise false. |mainloop| and |context| have to be from a valid Pulse
// threaded mainloop and the handle of the created stream will be returned by
// |stream|.
bool CreateInputStream(pa_threaded_mainloop* mainloop,
                       pa_context* context,
                       raw_ptr<pa_stream>* stream,
                       const AudioParameters& params,
                       const std::string& device_id,
                       pa_stream_notify_cb_t stream_callback,
                       void* user_data);

// Create a playback stream for the threaded mainloop, return true if success,
// otherwise false. This function will create a new Pulse threaded mainloop,
// and the handles of the mainloop, context and stream will be returned by
// |mainloop|, |context| and |stream|.
bool CreateOutputStream(raw_ptr<pa_threaded_mainloop>* mainloop,
                        raw_ptr<pa_context>* context,
                        raw_ptr<pa_stream>* stream,
                        const AudioParameters& params,
                        const std::string& device_id,
                        const std::string& app_name,
                        pa_stream_notify_cb_t stream_callback,
                        pa_stream_request_cb_t write_callback,
                        void* user_data);

// Utility functions to match up outputs and inputs.
std::string GetBusOfInput(pa_threaded_mainloop* mainloop,
                          pa_context* context,
                          const std::string& name);
std::string GetOutputCorrespondingTo(pa_threaded_mainloop* mainloop,
                                     pa_context* context,
                                     const std::string& bus);
std::string GetRealDefaultDeviceId(pa_threaded_mainloop* mainloop,
                                   pa_context* context,
                                   RequestType type);

// Get the name of the monitor associated with the given sink.
std::string GetMonitorSourceNameForSink(pa_threaded_mainloop* mainloop,
                                        pa_context* context,
                                        const std::string& sink_name);
}  // namespace pulse

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_PULSE_UTIL_H_
