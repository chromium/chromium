// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SILENT_SINK_SUSPENDER_H_
#define MEDIA_BASE_SILENT_SINK_SUSPENDER_H_

#include <stdint.h>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/fake_audio_worker.h"
#include "media/base/media_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// Helper class for suspending AudioRenderSink instances after silence has been
// detected for some time. When this is detected, the provided |sink_| is paused
// and a fake sink is injected to black hole the silent audio data and avoid
// physical hardware usage. Note: The transition from real to fake audio output
// and vice versa may result in some irregular Render() callbacks.
class MEDIA_EXPORT SilentSinkSuspender
    : public AudioRendererSink::RenderCallback {
 public:
  // |callback| is the true producer of audio data, |params| are the parameters
  // used to initialize |sink|, |sink| is the sink to monitor for idle, and
  // |worker| is the task runner to run the fake Render() callbacks on. The
  // amount of silence to allow before suspension is |silence_timeout|.
  SilentSinkSuspender(AudioRendererSink::RenderCallback* callback,
                      base::TimeDelta silence_timeout,
                      const AudioParameters& params,
                      scoped_refptr<AudioRendererSink> sink,
                      scoped_refptr<base::SingleThreadTaskRunner> worker);

  SilentSinkSuspender(const SilentSinkSuspender&) = delete;
  SilentSinkSuspender& operator=(const SilentSinkSuspender&) = delete;

  ~SilentSinkSuspender() override;

  // AudioRendererSink::RenderCallback implementation.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const AudioGlitchInfo& glitch_info,
             AudioBus* dest) override;
  void OnRenderError() override;

  // Cancels any outstanding callbacks and transitions. Subsequent playback will
  // be through the real sink until the suspend conditions are met again.
  void OnPaused();

  // Enables or disables silence detection. If disabled, |silence_timeout| will
  // be ignored and we will never transition to the fake sink. If we're already
  // on the fake sink when SetDetectSilence(false) is called, we'll transition
  // back to the real sink.
  void SetDetectSilence(bool detect_silence);

  bool IsUsingFakeSinkForTesting();

 private:
  // If |use_fake_sink| is true, pauses |sink_| and plays |fake_sink_|; if
  // false, pauses |fake_sink_| and plays |sink_|.
  void TransitionSinks(bool use_fake_sink);

  // Actual RenderCallback providing audio data to the output device.
  const raw_ptr<AudioRendererSink::RenderCallback> callback_;

  // Parameters used to construct |sink_|.
  const AudioParameters params_;

  // Sink monitored for silent output.
  scoped_refptr<AudioRendererSink> sink_;

  // Task runner this class is constructed on. Used to run TransitionSinks().
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Time when the silence starts.
  base::TimeTicks first_silence_time_;

  // Amount of time that can elapse before replacing |sink_| with |fake_sink_|.
  const base::TimeDelta silence_timeout_;

  // A fake audio sink object that consumes data when long period of silence
  // audio is detected. This object lives on |task_runner_| and will run
  // callbacks on RenderThreadImpl::GetMediaThreadTaskRunner().
  FakeAudioWorker fake_sink_;

  // AudioRendererSink::Pause() is not synchronous, so we need a lock to ensure
  // we don't have concurrent access to Render().
  base::Lock transition_lock_;

  // Whether audio output is directed to |fake_sink_|. Must only be used when
  // |transition_lock_| is held or both sinks are stopped.
  bool is_using_fake_sink_ GUARDED_BY(transition_lock_) = false;

  // Whether we're in the middle of a transition to or from |fake_sink_|. Must
  // only be used when |transition_lock_| is held or both sinks are stopped.
  bool is_transition_pending_ GUARDED_BY(transition_lock_) = false;

  // Whether we should detect silence and transition to the fake sink or not if
  // |silence_timeout_| elapses.
  bool detect_silence_ GUARDED_BY(transition_lock_) = true;

  // Buffers accumulated during the transition from |fake_sink_| to |sink_|.
  base::circular_deque<std::unique_ptr<AudioBus>> buffers_after_silence_;

  // A cancelable task that is posted to switch to or from the |fake_sink_|
  // after a period of silence or first non-silent audio respective. We do this
  // on Android to save battery consumption.
  base::CancelableRepeatingCallback<void(bool)> sink_transition_callback_;

  // Audio output delay at the moment when transition to |fake_sink_| starts.
  base::TimeDelta latest_output_delay_;

  // Audio output delay timestamp at the moment when transition to |fake_sink_|
  // starts.
  base::TimeTicks latest_output_delay_timestamp_;

  // Time when transition to |fake_sink_| starts.
  base::TimeTicks fake_sink_transition_time_;
};

}  // namespace media

#endif  // MEDIA_BASE_SILENT_SINK_SUSPENDER_H_
