// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_REALTIME_AUDIO_DESTINATION_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_REALTIME_AUDIO_DESTINATION_HANDLER_H_

#include <atomic>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_destination_node.h"
#include "third_party/blink/renderer/platform/audio/audio_callback_metric_reporter.h"
#include "third_party/blink/renderer/platform/audio/audio_destination.h"
#include "third_party/blink/renderer/platform/audio/audio_io_callback.h"

namespace blink {

class AudioContext;
class ExceptionState;
class WebAudioLatencyHint;
class WebAudioSinkDescriptor;

class RealtimeAudioDestinationHandler final
    : public AudioDestinationHandler,
      public AudioIOCallback,
      public base::SupportsWeakPtr<RealtimeAudioDestinationHandler> {
 public:
  static scoped_refptr<RealtimeAudioDestinationHandler> Create(
      AudioNode&,
      const WebAudioSinkDescriptor&,
      const WebAudioLatencyHint&,
      absl::optional<float> sample_rate);
  ~RealtimeAudioDestinationHandler() override;

  // For AudioHandler.
  void Dispose() override;
  void Initialize() override;
  void Uninitialize() override;
  void SetChannelCount(unsigned, ExceptionState&) override;
  double LatencyTime() const override { return 0; }
  double TailTime() const override { return 0; }
  bool RequiresTailProcessing() const final { return false; }

  // For AudioDestinationHandler.
  void StartRendering() override;
  void StopRendering() override;
  void Pause() override;
  void Resume() override;
  void RestartRendering() override;
  uint32_t MaxChannelCount() const override;
  double SampleRate() const override;
  void PrepareTaskRunnerForWorklet() override;

  // For AudioIOCallback. This is invoked by the platform audio destination to
  // get the next render quantum into `destination_bus` and update
  // `output_position`.
  void Render(AudioBus* destination_bus,
              uint32_t number_of_frames,
              const AudioIOPosition& output_position,
              const AudioCallbackMetric& metric) final;

  // Returns a hadrware callback buffer size from audio infra.
  uint32_t GetCallbackBufferSize() const;

  // Returns a given frames-per-buffer size from audio infra.
  int GetFramesPerBuffer() const;

  bool IsPullingAudioGraphAllowed() const {
    return allow_pulling_audio_graph_.load(std::memory_order_acquire);
  }

  // Sets the detect silence flag for the platform destination.
  void SetDetectSilence(bool detect_silence);

  // Sets the identifier for a new output device. Note that this will recreate
  // a new platform destination with the specified sink device. It also invokes
  // `callback` when the recreation is completed.
  void SetSinkDescriptor(const WebAudioSinkDescriptor& sink_descriptor,
                         media::OutputDeviceStatusCB callback);

 private:
  explicit RealtimeAudioDestinationHandler(
      AudioNode&,
      const WebAudioSinkDescriptor&,
      const WebAudioLatencyHint&,
      absl::optional<float> sample_rate);

  void CreatePlatformDestination();
  void StartPlatformDestination();
  void StopPlatformDestination();

  // Checks the current silent detection condition (e.g. the number of
  // automatic pull nodes) and flips the switch if necessary. Called within the
  // Render() method.
  void SetDetectSilenceIfNecessary(bool has_automatic_pull_nodes);

  // Should only be called from StartPlatformDestination.
  void EnablePullingAudioGraph() {
    allow_pulling_audio_graph_.store(true, std::memory_order_release);
  }

  // Should only be called from StopPlatformDestination.
  void DisablePullingAudioGraph() {
    allow_pulling_audio_graph_.store(false, std::memory_order_release);
  }

  // Stores a sink descriptor for sink transition.
  WebAudioSinkDescriptor sink_descriptor_;

  const WebAudioLatencyHint latency_hint_;

  // Holds the audio device thread that runs the real time audio context.
  scoped_refptr<AudioDestination> platform_destination_;

  absl::optional<float> sample_rate_;

  // If true, the audio graph will be pulled to get new data.  Otherwise, the
  // graph is not pulled, even if the audio thread is still running and
  // requesting data.
  //
  // Must be modified only in StartPlatformDestination (via
  // EnablePullingAudioGraph) or StopPlatformDestination (via
  // DisablePullingAudioGraph) .  This is modified only by the main threda and
  // the audio thread only reads this.
  std::atomic_bool allow_pulling_audio_graph_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Represents the current condition of silence detection. By default, the
  // silence detection is active.
  bool is_detecting_silence_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_REALTIME_AUDIO_DESTINATION_HANDLER_H_
