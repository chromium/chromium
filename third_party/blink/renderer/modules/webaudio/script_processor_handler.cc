// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/script_processor_handler.h"

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_processing_event.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/script_processor_node.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

ScriptProcessorHandler::ScriptProcessorHandler(
    AudioNode& node,
    float sample_rate,
    uint32_t buffer_size,
    uint32_t number_of_input_channels,
    uint32_t number_of_output_channels,
    const HeapVector<Member<AudioBuffer>>& input_buffers,
    const HeapVector<Member<AudioBuffer>>& output_buffers)
    : AudioHandler(kNodeTypeScriptProcessor, node, sample_rate),
      buffer_size_(buffer_size),
      number_of_input_channels_(number_of_input_channels),
      number_of_output_channels_(number_of_output_channels),
      internal_input_bus_(AudioBus::Create(
          number_of_input_channels,
          node.context()->GetDeferredTaskHandler().RenderQuantumFrames(),
          false)) {
  DCHECK_GE(buffer_size_,
            node.context()->GetDeferredTaskHandler().RenderQuantumFrames());
  DCHECK_LE(number_of_input_channels, BaseAudioContext::MaxNumberOfChannels());

  AddInput();
  AddOutput(number_of_output_channels);

  channel_count_ = number_of_input_channels;
  SetInternalChannelCountMode(V8ChannelCountMode::Enum::kExplicit);

  if (Context()->GetExecutionContext()) {
    task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
        TaskType::kMediaElementEvent);
  }

  for (uint32_t i = 0; i < 2; ++i) {
    shared_input_buffers_.push_back(
        input_buffers[i] ? input_buffers[i]->CreateSharedAudioBuffer()
                         : nullptr);
    shared_output_buffers_.push_back(
        output_buffers[i] ? output_buffers[i]->CreateSharedAudioBuffer()
                          : nullptr);
  }

  Initialize();

  LocalDOMWindow* window = To<LocalDOMWindow>(Context()->GetExecutionContext());
  if (window) {
    window->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kDeprecation,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "The ScriptProcessorNode is deprecated. Use AudioWorkletNode instead."
        " (https://bit.ly/audio-worklet)"));
  }
}

scoped_refptr<ScriptProcessorHandler> ScriptProcessorHandler::Create(
    AudioNode& node,
    float sample_rate,
    uint32_t buffer_size,
    uint32_t number_of_input_channels,
    uint32_t number_of_output_channels,
    const HeapVector<Member<AudioBuffer>>& input_buffers,
    const HeapVector<Member<AudioBuffer>>& output_buffers) {
  return base::AdoptRef(new ScriptProcessorHandler(
      node, sample_rate, buffer_size, number_of_input_channels,
      number_of_output_channels, input_buffers, output_buffers));
}

ScriptProcessorHandler::~ScriptProcessorHandler() {
  Uninitialize();
}

void ScriptProcessorHandler::Initialize() {
  if (IsInitialized()) {
    return;
  }
  AudioHandler::Initialize();
}

void ScriptProcessorHandler::Process(uint32_t frames_to_process) {
  TRACE_EVENT_BEGIN0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
                     "ScriptProcessorHandler::Process");

  // As in other AudioNodes, ScriptProcessorNode uses an AudioBus for its input
  // and output (i.e. `input_bus` and `output_bus`). Additionally, there is a
  // double-buffering for input and output that are exposed directly to
  // JavaScript (i.e. `.inputBuffer` and `.outputBuffer` in
  // AudioProcessingEvent). This node is the producer for `.inputBuffer` and the
  // consumer for `.outputBuffer`. The AudioProcessingEvent is the consumer of
  // `.inputBuffer` and the producer for `.outputBuffer`.

  scoped_refptr<AudioBus> input_bus = Input(0).Bus();
  AudioBus* output_bus = Output(0).Bus();

  {
    base::AutoTryLock try_locker(buffer_lock_);
    if (!try_locker.is_acquired()) {
      // Failed to acquire the output buffer, so output silence.
      TRACE_EVENT_INSTANT0(
          TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
          "ScriptProcessorHandler::Process - tryLock failed (output)",
          TRACE_EVENT_SCOPE_THREAD);
      TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
                       "ScriptProcessorHandler::Process");
      Output(0).Bus()->Zero();
      return;
    }

    uint32_t double_buffer_index = DoubleBufferIndex();
    DCHECK_LT(double_buffer_index, 2u);
    DCHECK_LT(double_buffer_index, shared_input_buffers_.size());
    DCHECK_LT(double_buffer_index, shared_output_buffers_.size());

    SharedAudioBuffer* shared_input_buffer =
        shared_input_buffers_.at(double_buffer_index).get();
    SharedAudioBuffer* shared_output_buffer =
        shared_output_buffers_.at(double_buffer_index).get();

    bool buffers_are_good =
        shared_output_buffer &&
        BufferSize() == shared_output_buffer->length() &&
        buffer_read_write_index_ + frames_to_process <= BufferSize();

    if (internal_input_bus_->NumberOfChannels()) {
      // If the number of input channels is zero, the zero length input buffer
      // is fine.
      buffers_are_good = buffers_are_good && shared_input_buffer &&
                         BufferSize() == shared_input_buffer->length();
    }

    DCHECK(buffers_are_good);

    // `BufferSize()` should be evenly divisible by `frames_to_process`.
    DCHECK_GT(frames_to_process, 0u);
    DCHECK_GE(BufferSize(), frames_to_process);
    DCHECK_EQ(BufferSize() % frames_to_process, 0u);

    uint32_t number_of_input_channels = internal_input_bus_->NumberOfChannels();
    uint32_t number_of_output_channels = output_bus->NumberOfChannels();
    DCHECK_EQ(number_of_input_channels, number_of_input_channels_);
    DCHECK_EQ(number_of_output_channels, number_of_output_channels_);

    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
                 "ScriptProcessorHandler::Process - data copy under lock",
                 "double_buffer_index", double_buffer_index);

    // It is possible that the length of `internal_input_bus_` and
    // `input_bus` can be different. See crbug.com/1189528.
    for (uint32_t i = 0; i < number_of_input_channels; ++i) {
      internal_input_bus_->SetChannelMemory(
          i,
          static_cast<float*>(shared_input_buffer->channels()[i].Data()) +
              buffer_read_write_index_,
          frames_to_process);
    }

    if (number_of_input_channels) {
      internal_input_bus_->CopyFrom(*input_bus);
    }

    for (uint32_t i = 0; i < number_of_output_channels; ++i) {
      float* destination = output_bus->Channel(i)->MutableData();
      const float* source =
          static_cast<float*>(shared_output_buffer->channels()[i].Data()) +
          buffer_read_write_index_;
      memcpy(destination, source, sizeof(float) * frames_to_process);
    }
  }

  // Update the buffer index for wrap-around.
  buffer_read_write_index_ =
      (buffer_read_write_index_ + frames_to_process) % BufferSize();
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
                       "ScriptProcessorHandler::Process",
                       TRACE_EVENT_SCOPE_THREAD, "buffer_read_write_index_",
                       buffer_read_write_index_);

  // Fire an event and swap buffers when `buffer_read_write_index_` wraps back
  // around to 0. It means the current input and output buffers are full.
  if (!buffer_read_write_index_) {
    if (Context()->HasRealtimeConstraint()) {
      // For a realtime context, fire an event and do not wait.
      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(&ScriptProcessorHandler::FireProcessEvent,
                              weak_ptr_factory_.GetWeakPtr(),
                              double_buffer_index_));
    } else {
      // For an offline context, wait until the script execution is finished.
      std::unique_ptr<base::WaitableEvent> waitable_event =
          std::make_unique<base::WaitableEvent>();
      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(
              &ScriptProcessorHandler::FireProcessEventForOfflineAudioContext,
              weak_ptr_factory_.GetWeakPtr(), double_buffer_index_,
              CrossThreadUnretained(waitable_event.get())));
      waitable_event->Wait();
    }

    // Update the double-buffering index.
    SwapBuffers();
  }

  TRACE_EVENT_END0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
                   "ScriptProcessorHandler::Process");
}

void ScriptProcessorHandler::FireProcessEvent(uint32_t double_buffer_index) {
  DCHECK(IsMainThread());

  if (!Context() || !Context()->GetExecutionContext()) {
    return;
  }

  DCHECK_LT(double_buffer_index, 2u);

  // Avoid firing the event if the document has already gone away.
  if (GetNode()) {
    // Calculate a playbackTime with the buffersize which needs to be processed
    // each time onaudioprocess is called.  The `.outputBuffer` being passed to
    // JS will be played after exhuasting previous `.outputBuffer` by
    // double-buffering.
    double playback_time = (Context()->CurrentSampleFrame() + buffer_size_) /
                           static_cast<double>(Context()->sampleRate());
    static_cast<ScriptProcessorNode*>(GetNode())->DispatchEvent(
        playback_time, double_buffer_index);
  }
}

void ScriptProcessorHandler::FireProcessEventForOfflineAudioContext(
    uint32_t double_buffer_index,
    base::WaitableEvent* waitable_event) {
  DCHECK(IsMainThread());

  if (!Context() || !Context()->GetExecutionContext()) {
    return;
  }

  DCHECK_LT(double_buffer_index, 2u);
  if (double_buffer_index > 1) {
    waitable_event->Signal();
    return;
  }

  if (GetNode()) {
    // We do not need a process lock here because the offline render thread
    // is locked by the waitable event.
    double playback_time = (Context()->CurrentSampleFrame() + buffer_size_) /
                           static_cast<double>(Context()->sampleRate());
    static_cast<ScriptProcessorNode*>(GetNode())->DispatchEvent(
        playback_time, double_buffer_index);
  }

  waitable_event->Signal();
}

bool ScriptProcessorHandler::RequiresTailProcessing() const {
  // Always return true since the tail and latency are never zero.
  return true;
}

double ScriptProcessorHandler::TailTime() const {
  return std::numeric_limits<double>::infinity();
}

double ScriptProcessorHandler::LatencyTime() const {
  return std::numeric_limits<double>::infinity();
}

void ScriptProcessorHandler::SetChannelCount(uint32_t channel_count,
                                             ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  if (channel_count != channel_count_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "channelCount cannot be changed from " +
                                          String::Number(channel_count_) +
                                          " to " +
                                          String::Number(channel_count));
  }
}

void ScriptProcessorHandler::SetChannelCountMode(
    V8ChannelCountMode::Enum mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DeferredTaskHandler::GraphAutoLocker locker(Context());

  if ((mode == V8ChannelCountMode::Enum::kMax) ||
      (mode == V8ChannelCountMode::Enum::kClampedMax)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "channelCountMode cannot be changed from 'explicit' to '" +
            V8ChannelCountMode(mode).AsString() + "'");
  }
}

}  // namespace blink
