// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/webaudio/audio_worklet_handler.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_param_descriptor.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_graph_tracer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_definition.h"
#include "third_party/blink/renderer/modules/webaudio/cross_thread_audio_worklet_processor_info.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

constexpr unsigned kDefaultNumberOfOutputChannels = 1;

}  // namespace

AudioWorkletHandler::AudioWorkletHandler(
    AudioNode& node,
    float sample_rate,
    String name,
    HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
    const AudioWorkletNodeOptions* options)
    : AudioHandler(kNodeTypeAudioWorklet, node, sample_rate),
      name_(name),
      param_handler_map_(param_handler_map) {
  DCHECK(IsMainThread());

  for (const auto& param_name : param_handler_map_.Keys()) {
    param_value_map_.Set(param_name,
                         std::make_unique<AudioFloatArray>(
                             GetDeferredTaskHandler().RenderQuantumFrames()));
  }

  for (unsigned i = 0; i < options->numberOfInputs(); ++i) {
    AddInput();
  }
  // The number of inputs does not change after the construction, so it is
  // safe to reserve the array capacity and size.
  inputs_.ReserveInitialCapacity(options->numberOfInputs());
  inputs_.resize(options->numberOfInputs());

  is_output_channel_count_given_ = options->hasOutputChannelCount();

  for (unsigned i = 0; i < options->numberOfOutputs(); ++i) {
    // If `options->outputChannelCount` unspecified, all outputs are mono.
    AddOutput(is_output_channel_count_given_ ? options->outputChannelCount()[i]
                                             : kDefaultNumberOfOutputChannels);
  }
  // Same for the outputs and the unconnected ones as well.
  outputs_.ReserveInitialCapacity(options->numberOfOutputs());
  outputs_.resize(options->numberOfOutputs());
  unconnected_outputs_.ReserveInitialCapacity(options->numberOfOutputs());
  unconnected_outputs_.resize(options->numberOfOutputs());

  if (Context()->GetExecutionContext()) {
    // Cross-thread tasks between AWN/AWP is okay to be throttled, thus
    // kMiscPlatformAPI. It is for post-creation/destruction chores.
    main_thread_task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
        TaskType::kMiscPlatformAPI);
    DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  }

  Initialize();
}

AudioWorkletHandler::~AudioWorkletHandler() {
  inputs_.clear();
  outputs_.clear();
  unconnected_outputs_.clear();
  param_handler_map_.clear();
  param_value_map_.clear();
  Uninitialize();
}

scoped_refptr<AudioWorkletHandler> AudioWorkletHandler::Create(
    AudioNode& node,
    float sample_rate,
    String name,
    HashMap<String, scoped_refptr<AudioParamHandler>> param_handler_map,
    const AudioWorkletNodeOptions* options) {
  return base::AdoptRef(new AudioWorkletHandler(node, sample_rate, name,
                                                param_handler_map, options));
}

void AudioWorkletHandler::Process(uint32_t frames_to_process) {
  DCHECK(Context()->IsAudioThread());

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "AudioWorkletHandler::Process");

  // The associated processor is not ready, finished, or might be in an error
  // state. If so, silence the connected outputs and return.
  if (!processor_ || processor_->hasErrorOccurred()) {
    for (unsigned i = 0; i < NumberOfOutputs(); ++i) {
      if (Output(i).IsConnectedDuringRendering()) {
        Output(i).Bus()->Zero();
      }
    }
    return;
  }

  // If the input or the output is not connected, inform the processor with
  // nullptr.
  for (unsigned i = 0; i < NumberOfInputs(); ++i) {
    inputs_[i] = Input(i).IsConnected() ? Input(i).Bus() : nullptr;
  }
  for (unsigned i = 0; i < NumberOfOutputs(); ++i) {
    if (!Output(i).IsConnectedDuringRendering()) {
      // If the output does not have an active outgoing connection, the handler
      // needs to provide an AudioBus for the AudioWorkletProcessor.
      if (!unconnected_outputs_[i] ||
          !unconnected_outputs_[i]->TopologyMatches(*Output(i).Bus())) {
        unconnected_outputs_[i] =
            AudioBus::Create(Output(i).Bus()->NumberOfChannels(),
                             GetDeferredTaskHandler().RenderQuantumFrames());
      }
      outputs_[i] = unconnected_outputs_[i];
    } else {
      // If there is one or more outgoing connection, use the AudioBus from the
      // output object.
      outputs_[i] = WrapRefCounted(Output(i).Bus());
    }
  }

  for (const auto& param_name : param_value_map_.Keys()) {
    auto* const param_handler = param_handler_map_.at(param_name);
    AudioFloatArray* param_values = param_value_map_.at(param_name);
    if (param_handler->HasSampleAccurateValues() &&
        param_handler->IsAudioRate()) {
      param_handler->CalculateSampleAccurateValues(
          param_values->Data(), static_cast<uint32_t>(frames_to_process));
    } else {
      std::fill(param_values->Data(),
                param_values->Data() + frames_to_process,
                param_handler->FinalValue());
    }
  }

  // Run the render code and check the return value or the state of processor.
  // If the return value is falsy, the processor's `Process()` function
  // won't be called again.
  if (!processor_->Process(inputs_, outputs_, param_value_map_) ||
      processor_->hasErrorOccurred()) {
    FinishProcessorOnRenderThread();
  }
}

void AudioWorkletHandler::CheckNumberOfChannelsForInput(AudioNodeInput* input) {
  DCHECK(Context()->IsAudioThread());
  Context()->AssertGraphOwner();
  DCHECK(input);

  // Dynamic channel count only works when the node has 1 input, 1 output and
  // the output channel count is not given. Otherwise the channel count(s)
  // should not be dynamically changed.
  if (NumberOfInputs() == 1 && NumberOfOutputs() == 1 &&
      !is_output_channel_count_given_) {
    DCHECK_EQ(input, &Input(0));
    unsigned number_of_input_channels = Input(0).NumberOfChannels();
    if (number_of_input_channels != Output(0).NumberOfChannels()) {
      // This will propagate the channel count to any nodes connected further
      // downstream in the graph.
      Output(0).SetNumberOfChannels(number_of_input_channels);
    }
  }

  AudioHandler::CheckNumberOfChannelsForInput(input);
  UpdatePullStatusIfNeeded();
}

void AudioWorkletHandler::UpdatePullStatusIfNeeded() {
  Context()->AssertGraphOwner();

  bool is_output_connected = false;
  for (unsigned i = 0; i < NumberOfOutputs(); ++i) {
    if (Output(i).IsConnected()) {
      is_output_connected = true;
      break;
    }
  }

  // If no output is connected, add the node to the automatic pull list.
  // Otherwise, remove it out of the list.
  if (!is_output_connected) {
    Context()->GetDeferredTaskHandler().AddAutomaticPullNode(this);
  } else {
    Context()->GetDeferredTaskHandler().RemoveAutomaticPullNode(this);
  }
}

double AudioWorkletHandler::TailTime() const {
  DCHECK(Context()->IsAudioThread());
  return tail_time_;
}

void AudioWorkletHandler::SetProcessorOnRenderThread(
    AudioWorkletProcessor* processor) {
  // TODO(crbug.com/1071917): unify the thread ID check. The thread ID for this
  // call may be different from `Context()->IsAudiothread()`.
  DCHECK(!IsMainThread());

  // `processor` can be `nullptr` when the invocation of user-supplied
  // constructor fails. That failure fires at the node's `.onprocessorerror`
  // event handler.
  if (processor) {
    processor_ = processor;
  } else {
    PostCrossThreadTask(
        *main_thread_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &AudioWorkletHandler::NotifyProcessorError,
            weak_ptr_factory_.GetWeakPtr(),
            AudioWorkletProcessorErrorState::kConstructionError));
  }
}

void AudioWorkletHandler::FinishProcessorOnRenderThread() {
  DCHECK(Context()->IsAudioThread());

  // If the user-supplied code is not runnable (i.e. threw an exception)
  // anymore after the process() call above. Invoke error on the main thread.
  AudioWorkletProcessorErrorState error_state = processor_->GetErrorState();
  if (error_state == AudioWorkletProcessorErrorState::kProcessError) {
    PostCrossThreadTask(
        *main_thread_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&AudioWorkletHandler::NotifyProcessorError,
                            weak_ptr_factory_.GetWeakPtr(), error_state));
  }

  // After this point, the handler has no more pending activity and is ready for
  // GC.
  Context()->NotifySourceNodeFinishedProcessing(this);
  processor_.Clear();
  tail_time_ = 0;

  // The processor is cleared, so queue a task to mark this handler (and its
  // associated AudioWorkletNode) is ready for GC.
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &AudioWorkletHandler::MarkProcessorInactiveOnMainThread,
          weak_ptr_factory_.GetWeakPtr()));
}

void AudioWorkletHandler::NotifyProcessorError(
    AudioWorkletProcessorErrorState error_state) {
  DCHECK(IsMainThread());
  if (!Context() || !Context()->GetExecutionContext() || !GetNode()) {
    return;
  }

  static_cast<AudioWorkletNode*>(GetNode())->FireProcessorError(error_state);
}

void AudioWorkletHandler::MarkProcessorInactiveOnMainThread() {
  DCHECK(IsMainThread());

  is_processor_active_ = false;
}

}  // namespace blink
