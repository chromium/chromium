/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"

#include "third_party/blink/renderer/modules/webaudio/audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"

namespace blink {

scoped_refptr<RealtimeAudioDestinationHandler>
RealtimeAudioDestinationHandler::Create(AudioNode& node,
                                        const WebAudioLatencyHint& latency_hint,
                                        base::Optional<float> sample_rate) {
  return base::AdoptRef(
      new RealtimeAudioDestinationHandler(node, latency_hint, sample_rate));
}

RealtimeAudioDestinationHandler::RealtimeAudioDestinationHandler(
    AudioNode& node,
    const WebAudioLatencyHint& latency_hint,
    base::Optional<float> sample_rate)
    : AudioDestinationHandler(node),
      latency_hint_(latency_hint),
      sample_rate_(sample_rate),
      allow_pulling_audio_graph_(false),
      task_runner_(Context()->GetExecutionContext()->GetTaskRunner(
          TaskType::kInternalMediaRealTime)) {
  // Node-specific default channel count and mixing rules.
  channel_count_ = 2;
  SetInternalChannelCountMode(kExplicit);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);
}

RealtimeAudioDestinationHandler::~RealtimeAudioDestinationHandler() {
  DCHECK(!IsInitialized());
}

void RealtimeAudioDestinationHandler::Dispose() {
  Uninitialize();
  AudioDestinationHandler::Dispose();
}

void RealtimeAudioDestinationHandler::Initialize() {
  DCHECK(IsMainThread());

  CreatePlatformDestination();
  AudioHandler::Initialize();
}

void RealtimeAudioDestinationHandler::Uninitialize() {
  DCHECK(IsMainThread());

  // It is possible that the handler is already uninitialized.
  if (!IsInitialized()) {
    return;
  }

  StopPlatformDestination();
  AudioHandler::Uninitialize();
}

void RealtimeAudioDestinationHandler::SetChannelCount(
    unsigned channel_count,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // The channelCount for the input to this node controls the actual number of
  // channels we send to the audio hardware. It can only be set if the number
  // is less than the number of hardware channels.
  if (channel_count > MaxChannelCount()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        ExceptionMessages::IndexOutsideRange<unsigned>(
            "channel count", channel_count, 1,
            ExceptionMessages::kInclusiveBound, MaxChannelCount(),
            ExceptionMessages::kInclusiveBound));
    return;
  }

  uint32_t old_channel_count = this->ChannelCount();
  AudioHandler::SetChannelCount(channel_count, exception_state);

  // Stop, re-create and start the destination to apply the new channel count.
  if (this->ChannelCount() != old_channel_count &&
      !exception_state.HadException()) {
    StopPlatformDestination();
    CreatePlatformDestination();
    StartPlatformDestination();
  }
}

void RealtimeAudioDestinationHandler::StartRendering() {
  DCHECK(IsMainThread());

  StartPlatformDestination();
}

void RealtimeAudioDestinationHandler::StopRendering() {
  DCHECK(IsMainThread());

  StopPlatformDestination();
}

void RealtimeAudioDestinationHandler::Pause() {
  DCHECK(IsMainThread());
  if (platform_destination_) {
    platform_destination_->Pause();
  }
}

void RealtimeAudioDestinationHandler::Resume() {
  DCHECK(IsMainThread());
  if (platform_destination_) {
    platform_destination_->Resume();
  }
}

void RealtimeAudioDestinationHandler::RestartRendering() {
  DCHECK(IsMainThread());

  StopRendering();
  StartRendering();
}

uint32_t RealtimeAudioDestinationHandler::MaxChannelCount() const {
  return AudioDestination::MaxChannelCount();
}

double RealtimeAudioDestinationHandler::SampleRate() const {
  // This can be accessed from both threads (main and audio), so it is
  // possible that |platform_destination_| is not fully functional when it
  // is accssed by the audio thread.
  return platform_destination_ ? platform_destination_->SampleRate() : 0;
}

void RealtimeAudioDestinationHandler::Render(
    AudioBus* destination_bus,
    uint32_t number_of_frames,
    const AudioIOPosition& output_position,
    const AudioCallbackMetric& metric) {
  TRACE_EVENT0("webaudio", "RealtimeAudioDestinationHandler::Render");

  // Denormals can seriously hurt performance of audio processing. This will
  // take care of all AudioNode processes within this scope.
  DenormalDisabler denormal_disabler;

  AudioContext* context = static_cast<AudioContext*>(Context());

  // A sanity check for the associated context, but this does not guarantee the
  // safe execution of the subsequence operations because the hanlder holds
  // the context as |UntracedMember| and it can go away anytime.
  DCHECK(context);
  if (!context) {
    return;
  }

  context->GetDeferredTaskHandler().SetAudioThreadToCurrentThread();

  // If this node is not initialized yet, pass silence to the platform audio
  // destination. It is for the case where this node is in the middle of
  // tear-down process.
  if (!IsInitialized()) {
    destination_bus->Zero();
    return;
  }

  context->HandlePreRenderTasks(&output_position, &metric);

  // Only pull on the audio graph if we have not stopped the destination.  It
  // takes time for the destination to stop, but we want to stop pulling before
  // the destination has actually stopped.
  if (IsPullingAudioGraphAllowed()) {
    // Renders the graph by pulling all the inputs to this node. This will in
    // turn pull on their inputs, all the way backwards through the graph.
    scoped_refptr<AudioBus> rendered_bus =
        Input(0).Pull(destination_bus, number_of_frames);

    DCHECK(rendered_bus);
    if (!rendered_bus) {
      // AudioNodeInput might be in the middle of destruction. Then the internal
      // summing bus will return as nullptr. Then zero out the output.
      destination_bus->Zero();
    } else if (rendered_bus != destination_bus) {
      // In-place processing was not possible. Copy the rendered result to the
      // given |destination_bus| buffer.
      destination_bus->CopyFrom(*rendered_bus);
    }
  } else {
    destination_bus->Zero();
  }

  // Processes "automatic" nodes that are not connected to anything. This can
  // be done after copying because it does not affect the rendered result.
  context->GetDeferredTaskHandler().ProcessAutomaticPullNodes(number_of_frames);

  context->HandlePostRenderTasks();

  context->HandleAudibility(destination_bus);

  // Advances the current sample-frame.
  AdvanceCurrentSampleFrame(number_of_frames);

  context->UpdateWorkletGlobalScopeOnRenderingThread();

  SetDetectSilenceIfNecessary(
      context->GetDeferredTaskHandler().HasAutomaticPullNodes());
}

void RealtimeAudioDestinationHandler::SetDetectSilenceIfNecessary(
    bool has_automatic_pull_nodes) {
  // When there is no automatic pull nodes, or the destination has an active
  // input connection, the silence detection should be turned on.
  bool needs_silence_detection =
      !has_automatic_pull_nodes || Input(0).IsConnected();

  // Post a cross-thread task only when the detecting condition has changed.
  if (is_detecting_silence_ != needs_silence_detection) {
    PostCrossThreadTask(*task_runner_, FROM_HERE,
        CrossThreadBindOnce(&RealtimeAudioDestinationHandler::SetDetectSilence,
                            AsWeakPtr(), needs_silence_detection));
    is_detecting_silence_ = needs_silence_detection;
  }
}

void RealtimeAudioDestinationHandler::SetDetectSilence(bool detect_silence) {
  DCHECK(IsMainThread());

  platform_destination_->SetDetectSilence(detect_silence);
}

uint32_t RealtimeAudioDestinationHandler::GetCallbackBufferSize() const {
  DCHECK(IsMainThread());
  DCHECK(IsInitialized());

  return platform_destination_->CallbackBufferSize();
}

int RealtimeAudioDestinationHandler::GetFramesPerBuffer() const {
  DCHECK(IsMainThread());
  DCHECK(IsInitialized());

  return platform_destination_ ? platform_destination_->FramesPerBuffer() : 0;
}

void RealtimeAudioDestinationHandler::CreatePlatformDestination() {
  platform_destination_ = AudioDestination::Create(*this, ChannelCount(),
                                                   latency_hint_, sample_rate_);
}

void RealtimeAudioDestinationHandler::StartPlatformDestination() {
  DCHECK(IsMainThread());

  if (platform_destination_->IsPlaying()) {
    return;
  }

  AudioWorklet* audio_worklet = Context()->audioWorklet();
  if (audio_worklet && audio_worklet->IsReady()) {
    // This task runner is only used to fire the audio render callback, so it
    // MUST not be throttled to avoid potential audio glitch.
    platform_destination_->StartWithWorkletTaskRunner(
        audio_worklet->GetMessagingProxy()
            ->GetBackingWorkerThread()
            ->GetTaskRunner(TaskType::kInternalMediaRealTime));
  } else {
    platform_destination_->Start();
  }

  // Allow the graph to be pulled once the destination actually starts
  // requesting data.
  EnablePullingAudioGraph();
}

void RealtimeAudioDestinationHandler::StopPlatformDestination() {
  DCHECK(IsMainThread());

  // Stop pulling on the graph, even if the destination is still requesting data
  // for a while. (It may take a bit of time for the destination to stop.)
  DisablePullingAudioGraph();

  if (platform_destination_->IsPlaying()) {
    platform_destination_->Stop();
  }
}

// -----------------------------------------------------------------------------

RealtimeAudioDestinationNode::RealtimeAudioDestinationNode(
    AudioContext& context,
    const WebAudioLatencyHint& latency_hint,
    base::Optional<float> sample_rate)
    : AudioDestinationNode(context) {
  SetHandler(RealtimeAudioDestinationHandler::Create(*this, latency_hint,
                                                     sample_rate));
}

RealtimeAudioDestinationNode* RealtimeAudioDestinationNode::Create(
    AudioContext* context,
    const WebAudioLatencyHint& latency_hint,
    base::Optional<float> sample_rate) {
  return MakeGarbageCollected<RealtimeAudioDestinationNode>(
      *context, latency_hint, sample_rate);
}

}  // namespace blink
