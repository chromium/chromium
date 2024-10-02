// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/offline_audio_destination_handler.h"

#include <algorithm>

#include "base/trace_event/typed_macros.h"
#include "media/base/audio_glitch_info.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/audio/hrtf_database_loader.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

OfflineAudioDestinationHandler::OfflineAudioDestinationHandler(
    AudioNode& node,
    unsigned number_of_channels,
    uint32_t frames_to_process,
    float sample_rate)
    : AudioDestinationHandler(node),
      frames_to_process_(frames_to_process),
      number_of_channels_(number_of_channels),
      sample_rate_(sample_rate),
      main_thread_task_runner_(Context()->GetExecutionContext()->GetTaskRunner(
          TaskType::kInternalMedia)) {
  DCHECK(main_thread_task_runner_->BelongsToCurrentThread());

  channel_count_ = number_of_channels;
  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kExplicit);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);
}

scoped_refptr<OfflineAudioDestinationHandler>
OfflineAudioDestinationHandler::Create(AudioNode& node,
                                       unsigned number_of_channels,
                                       uint32_t frames_to_process,
                                       float sample_rate) {
  return base::AdoptRef(new OfflineAudioDestinationHandler(
      node, number_of_channels, frames_to_process, sample_rate));
}

OfflineAudioDestinationHandler::~OfflineAudioDestinationHandler() {
  DCHECK(!IsInitialized());
}

void OfflineAudioDestinationHandler::Dispose() {
  Uninitialize();
  AudioDestinationHandler::Dispose();
}

void OfflineAudioDestinationHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }

  AudioHandler::Initialize();
}

void OfflineAudioDestinationHandler::Uninitialize() {
  if (!IsInitialized()) {
    return;
  }

  // See https://crbug.com/1110035 and https://crbug.com/1080821. Resetting the
  // thread unique pointer multiple times or not-resetting at all causes a
  // mysterious CHECK failure or a crash.
  if (render_thread_) {
    render_thread_.reset();
  }

  AudioHandler::Uninitialize();
}

OfflineAudioContext* OfflineAudioDestinationHandler::Context() const {
  return static_cast<OfflineAudioContext*>(AudioDestinationHandler::Context());
}

uint32_t OfflineAudioDestinationHandler::MaxChannelCount() const {
  return channel_count_;
}

void OfflineAudioDestinationHandler::StartRendering() {
  DCHECK(IsMainThread());
  DCHECK(shared_render_target_);
  DCHECK(render_thread_task_runner_);

  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
              "OfflineAudioDestinationHandler::StartRendering", "this",
              reinterpret_cast<void*>(this));

  // Rendering was not started. Starting now.
  if (!is_rendering_started_) {
    is_rendering_started_ = true;
    PostCrossThreadTask(
        *render_thread_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &OfflineAudioDestinationHandler::StartOfflineRendering,
            WrapRefCounted(this)));
    return;
  }

  // Rendering is already started, which implicitly means we resume the
  // rendering by calling `DoOfflineRendering()` on the render thread.
  PostCrossThreadTask(
      *render_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&OfflineAudioDestinationHandler::DoOfflineRendering,
                          WrapRefCounted(this)));
}

void OfflineAudioDestinationHandler::StopRendering() {
  // offline audio rendering CANNOT BE stopped by JavaScript.
  NOTREACHED_IN_MIGRATION();
}

void OfflineAudioDestinationHandler::Pause() {
  NOTREACHED_IN_MIGRATION();
}

void OfflineAudioDestinationHandler::Resume() {
  NOTREACHED_IN_MIGRATION();
}

void OfflineAudioDestinationHandler::InitializeOfflineRenderThread(
    AudioBuffer* render_target) {
  DCHECK(IsMainThread());

  shared_render_target_ = render_target->CreateSharedAudioBuffer();
  render_bus_ =
      AudioBus::Create(render_target->numberOfChannels(),
                       GetDeferredTaskHandler().RenderQuantumFrames());
  DCHECK(render_bus_);

  PrepareTaskRunnerForRendering();
}

void OfflineAudioDestinationHandler::StartOfflineRendering() {
  DCHECK(!IsMainThread());
  DCHECK(render_bus_);

  bool is_audio_context_initialized = Context()->IsDestinationInitialized();
  DCHECK(is_audio_context_initialized);

  DCHECK_EQ(render_bus_->NumberOfChannels(),
            shared_render_target_->numberOfChannels());
  DCHECK_GE(render_bus_->length(),
            GetDeferredTaskHandler().RenderQuantumFrames());

  // Start rendering.
  DoOfflineRendering();
}

void OfflineAudioDestinationHandler::DoOfflineRendering() {
  DCHECK(!IsMainThread());
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
              "OfflineAudioDestinationHandler::DoOfflineRendering", "this",
              reinterpret_cast<void*>(this));

  unsigned number_of_channels = shared_render_target_->numberOfChannels();
  Vector<float*> destinations;
  destinations.ReserveInitialCapacity(number_of_channels);
  for (unsigned i = 0; i < number_of_channels; ++i) {
    destinations.push_back(
        static_cast<float*>(shared_render_target_->channels()[i].Data()));
  }

  // If there is more to process and there is no suspension at the moment,
  // do continue to render quanta. Then calling OfflineAudioContext.resume()
  // will pick up the render loop again from where it was suspended.
  while (frames_to_process_ > 0) {
    // Suspend the rendering if a scheduled suspend found at the current
    // sample frame. Otherwise render one quantum.
    if (RenderIfNotSuspended(nullptr, render_bus_.get(),
                             GetDeferredTaskHandler().RenderQuantumFrames())) {
      return;
    }

    uint32_t frames_available_to_copy = std::min(
        frames_to_process_, GetDeferredTaskHandler().RenderQuantumFrames());

    for (unsigned channel_index = 0; channel_index < number_of_channels;
         ++channel_index) {
      const float* source = render_bus_->Channel(channel_index)->Data();
      memcpy(destinations[channel_index] + frames_processed_, source,
             sizeof(float) * frames_available_to_copy);
    }

    frames_processed_ += frames_available_to_copy;

    DCHECK_GE(frames_to_process_, frames_available_to_copy);
    frames_to_process_ -= frames_available_to_copy;
  }

  DCHECK_EQ(frames_to_process_, 0u);
  FinishOfflineRendering();
}

void OfflineAudioDestinationHandler::SuspendOfflineRendering() {
  DCHECK(!IsMainThread());

  // The actual rendering has been suspended. Notify the context.
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&OfflineAudioDestinationHandler::NotifySuspend,
                          GetWeakPtr(), Context()->CurrentSampleFrame()));
}

void OfflineAudioDestinationHandler::FinishOfflineRendering() {
  DCHECK(!IsMainThread());

  // The actual rendering has been completed. Notify the context.
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&OfflineAudioDestinationHandler::NotifyComplete,
                          GetWeakPtr()));
}

void OfflineAudioDestinationHandler::NotifySuspend(size_t frame) {
  DCHECK(IsMainThread());

  if (!IsExecutionContextDestroyed() && Context()) {
    Context()->ResolveSuspendOnMainThread(frame);
  }
}

void OfflineAudioDestinationHandler::NotifyComplete() {
  DCHECK(IsMainThread());

  render_thread_.reset();

  // If the execution context has been destroyed, there's nowhere to send the
  // notification, so just return.
  if (IsExecutionContextDestroyed()) {
    return;
  }

  // The OfflineAudioContext might be gone.
  if (Context() && Context()->GetExecutionContext()) {
    Context()->FireCompletionEvent();
  }
}

bool OfflineAudioDestinationHandler::RenderIfNotSuspended(
    AudioBus* source_bus,
    AudioBus* destination_bus,
    uint32_t number_of_frames) {
  // We don't want denormals slowing down any of the audio processing
  // since they can very seriously hurt performance.
  // This will take care of all AudioNodes because they all process within this
  // scope.
  DenormalDisabler denormal_disabler;

  // Need to check if the context actually alive. Otherwise the subsequent
  // steps will fail. If the context is not alive somehow, return immediately
  // and do nothing.
  //
  // TODO(hongchan): because the context can go away while rendering, so this
  // check cannot guarantee the safe execution of the following steps.
  DCHECK(Context());
  if (!Context()) {
    return false;
  }

  Context()->GetDeferredTaskHandler().SetAudioThreadToCurrentThread();

  // If the destination node is not initialized, pass the silence to the final
  // audio destination (one step before the FIFO). This check is for the case
  // where the destination is in the middle of tearing down process.
  if (!IsInitialized()) {
    destination_bus->Zero();
    return false;
  }

  // Take care pre-render tasks at the beginning of each render quantum. Then
  // it will stop the rendering loop if the context needs to be suspended
  // at the beginning of the next render quantum.
  if (Context()->HandlePreRenderTasks(number_of_frames, nullptr, nullptr,
                                      base::TimeDelta(),
                                      media::AudioGlitchInfo())) {
    SuspendOfflineRendering();
    return true;
  }

  DCHECK_GE(NumberOfInputs(), 1u);

  // This will cause the node(s) connected to us to process, which in turn will
  // pull on their input(s), all the way backwards through the rendering graph.
  scoped_refptr<AudioBus> rendered_bus =
      Input(0).Pull(destination_bus, number_of_frames);

  if (!rendered_bus) {
    destination_bus->Zero();
  } else if (rendered_bus != destination_bus) {
    // in-place processing was not possible - so copy
    destination_bus->CopyFrom(*rendered_bus);
  }

  // Process nodes which need a little extra help because they are not connected
  // to anything, but still need to process.
  Context()->GetDeferredTaskHandler().ProcessAutomaticPullNodes(
      number_of_frames);

  // Let the context take care of any business at the end of each render
  // quantum.
  Context()->HandlePostRenderTasks();

  // Advance current sample-frame.
  AdvanceCurrentSampleFrame(number_of_frames);

  Context()->UpdateWorkletGlobalScopeOnRenderingThread();

  return false;
}

void OfflineAudioDestinationHandler::PrepareTaskRunnerForRendering() {
  DCHECK(IsMainThread());

  AudioWorklet* audio_worklet = Context()->audioWorklet();
  if (audio_worklet && audio_worklet->IsReady()) {
    if (!render_thread_) {
      // The context (re)started with the AudioWorklet mode. Assign the task
      // runner only when it is not set yet.
      if (!render_thread_task_runner_) {
        render_thread_task_runner_ =
            audio_worklet->GetMessagingProxy()
                ->GetBackingWorkerThread()
                ->GetTaskRunner(TaskType::kMiscPlatformAPI);
      }
    } else {
      // The AudioWorklet is activated and the render task runner should be
      // changed.
      render_thread_ = nullptr;
      render_thread_task_runner_ =
          audio_worklet->GetMessagingProxy()
              ->GetBackingWorkerThread()
              ->GetTaskRunner(TaskType::kMiscPlatformAPI);
    }
  } else {
    if (!render_thread_) {
      // The context started from the non-AudioWorklet mode.
      render_thread_ = NonMainThread::CreateThread(
          ThreadCreationParams(ThreadType::kOfflineAudioRenderThread));
      render_thread_task_runner_ = render_thread_->GetTaskRunner();
    }
  }

  // The task runner MUST be valid at this point.
  DCHECK(render_thread_task_runner_);
}

void OfflineAudioDestinationHandler::RestartRendering() {
  DCHECK(IsMainThread());

  // The rendering thread might have been changed, so we need to set up the
  // task runner again.
  PrepareTaskRunnerForRendering();
}

}  // namespace blink
