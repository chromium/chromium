/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/webaudio/script_processor_node.h"

#include <memory>

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_processing_event.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
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
      double_buffer_index_(0),
      buffer_size_(buffer_size),
      buffer_read_write_index_(0),
      number_of_input_channels_(number_of_input_channels),
      number_of_output_channels_(number_of_output_channels),
      internal_input_bus_(
          AudioBus::Create(number_of_input_channels,
                           audio_utilities::kRenderQuantumFrames,
                           false)) {
  DCHECK_GE(buffer_size_, audio_utilities::kRenderQuantumFrames);
  DCHECK_LE(number_of_input_channels, BaseAudioContext::MaxNumberOfChannels());

  AddInput();
  AddOutput(number_of_output_channels);

  channel_count_ = number_of_input_channels;
  SetInternalChannelCountMode(kExplicit);

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
  if (IsInitialized())
    return;
  AudioHandler::Initialize();
}

void ScriptProcessorHandler::Process(uint32_t frames_to_process) {
  // Discussion about inputs and outputs:
  // As in other AudioNodes, ScriptProcessorNode uses an AudioBus for its input
  // and output (see inputBus and outputBus below).  Additionally, there is a
  // double-buffering for input and output which is exposed directly to
  // JavaScript (see inputBuffer and outputBuffer below).  This node is the
  // producer for inputBuffer and the consumer for outputBuffer.  The JavaScript
  // code is the consumer of inputBuffer and the producer for outputBuffer.

  // Get input and output busses.
  AudioBus* input_bus = Input(0).Bus();
  AudioBus* output_bus = Output(0).Bus();

  // Get input and output buffers. We double-buffer both the input and output
  // sides.
  uint32_t double_buffer_index = this->DoubleBufferIndex();
  DCHECK_LT(double_buffer_index, 2u);
  DCHECK_LT(double_buffer_index, shared_input_buffers_.size());
  DCHECK_LT(double_buffer_index, shared_output_buffers_.size());

  SharedAudioBuffer* shared_input_buffer =
      shared_input_buffers_.at(double_buffer_index).get();
  SharedAudioBuffer* shared_output_buffer =
      shared_output_buffers_.at(double_buffer_index).get();

  // Check the consistency of input and output buffers.
  uint32_t number_of_input_channels = internal_input_bus_->NumberOfChannels();
  bool buffers_are_good =
      shared_output_buffer && BufferSize() == shared_output_buffer->length() &&
      buffer_read_write_index_ + frames_to_process <= BufferSize();

  // If the number of input channels is zero, it's ok to have inputBuffer = 0.
  if (internal_input_bus_->NumberOfChannels()) {
    buffers_are_good = buffers_are_good && shared_input_buffer &&
                       BufferSize() == shared_input_buffer->length();
  }

  DCHECK(buffers_are_good);

  // We assume that bufferSize() is evenly divisible by framesToProcess - should
  // always be true, but we should still check.
  DCHECK_GT(frames_to_process, 0u);
  DCHECK_GE(BufferSize(), frames_to_process);
  DCHECK_EQ(BufferSize() % frames_to_process, 0u);

  uint32_t number_of_output_channels = output_bus->NumberOfChannels();

  DCHECK_EQ(number_of_input_channels, number_of_input_channels_);
  DCHECK_EQ(number_of_output_channels, number_of_output_channels_);

  for (uint32_t i = 0; i < number_of_input_channels; ++i)
    internal_input_bus_->SetChannelMemory(
        i,
        static_cast<float*>(shared_input_buffer->channels()[i].Data()) +
            buffer_read_write_index_,
        frames_to_process);

  if (number_of_input_channels)
    internal_input_bus_->CopyFrom(*input_bus);

  // Copy from the output buffer to the output.
  for (uint32_t i = 0; i < number_of_output_channels; ++i) {
    memcpy(output_bus->Channel(i)->MutableData(),
           static_cast<float*>(shared_output_buffer->channels()[i].Data()) +
               buffer_read_write_index_,
           sizeof(float) * frames_to_process);
  }

  // Update the buffering index.
  buffer_read_write_index_ =
      (buffer_read_write_index_ + frames_to_process) % BufferSize();

  // m_bufferReadWriteIndex will wrap back around to 0 when the current input
  // and output buffers are full.
  // When this happens, fire an event and swap buffers.
  if (!buffer_read_write_index_) {
    // Avoid building up requests on the main thread to fire process events when
    // they're not being handled.  This could be a problem if the main thread is
    // very busy doing other things and is being held up handling previous
    // requests.  The audio thread can't block on this lock, so we call
    // tryLock() instead.
    MutexTryLocker try_locker(process_event_lock_);
    if (!try_locker.Locked()) {
      // We're late in handling the previous request. The main thread must be
      // very busy.  The best we can do is clear out the buffer ourself here.
      shared_output_buffer->Zero();
    } else {
      // With the realtime context, execute the script code asynchronously
      // and do not wait.
      if (Context()->HasRealtimeConstraint()) {
        // Fire the event on the main thread with the appropriate buffer
        // index.
        PostCrossThreadTask(
            *task_runner_, FROM_HERE,
            CrossThreadBindOnce(&ScriptProcessorHandler::FireProcessEvent,
                                WrapRefCounted(this), double_buffer_index_));
      } else {
        // If this node is in the offline audio context, use the
        // waitable event to synchronize to the offline rendering thread.
        std::unique_ptr<base::WaitableEvent> waitable_event =
            std::make_unique<base::WaitableEvent>();

        PostCrossThreadTask(
            *task_runner_, FROM_HERE,
            CrossThreadBindOnce(
                &ScriptProcessorHandler::FireProcessEventForOfflineAudioContext,
                WrapRefCounted(this), double_buffer_index_,
                CrossThreadUnretained(waitable_event.get())));

        // Okay to block the offline audio rendering thread since it is
        // not the actual audio device thread.
        waitable_event->Wait();
      }
    }

    SwapBuffers();
  }
}

void ScriptProcessorHandler::FireProcessEvent(uint32_t double_buffer_index) {
  DCHECK(IsMainThread());

  if (!Context() || !Context()->GetExecutionContext())
    return;

  DCHECK_LT(double_buffer_index, 2u);

  // Avoid firing the event if the document has already gone away.
  if (GetNode()) {
    // This synchronizes with process().
    MutexLocker process_locker(process_event_lock_);

    // Calculate a playbackTime with the buffersize which needs to be processed
    // each time onaudioprocess is called.  The outputBuffer being passed to JS
    // will be played after exhuasting previous outputBuffer by
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

  if (!Context() || !Context()->GetExecutionContext())
    return;

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
  BaseAudioContext::GraphAutoLocker locker(Context());

  if (channel_count != channel_count_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "channelCount cannot be changed from " +
                                          String::Number(channel_count_) +
                                          " to " +
                                          String::Number(channel_count));
  }
}

void ScriptProcessorHandler::SetChannelCountMode(
    const String& mode,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  if ((mode == "max") || (mode == "clamped-max")) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "channelCountMode cannot be changed from 'explicit' to '" + mode + "'");
  }
}

// ----------------------------------------------------------------

ScriptProcessorNode::ScriptProcessorNode(BaseAudioContext& context,
                                         float sample_rate,
                                         uint32_t buffer_size,
                                         uint32_t number_of_input_channels,
                                         uint32_t number_of_output_channels)
    : AudioNode(context) {
  // Regardless of the allowed buffer sizes, we still need to process at the
  // granularity of the AudioNode.
  if (buffer_size < audio_utilities::kRenderQuantumFrames)
    buffer_size = audio_utilities::kRenderQuantumFrames;

  // Create double buffers on both the input and output sides.
  // These AudioBuffers will be directly accessed in the main thread by
  // JavaScript.
  for (uint32_t i = 0; i < 2; ++i) {
    AudioBuffer* input_buffer =
        number_of_input_channels ? AudioBuffer::Create(number_of_input_channels,
                                                       buffer_size, sample_rate)
                                 : nullptr;
    AudioBuffer* output_buffer =
        number_of_output_channels
            ? AudioBuffer::Create(number_of_output_channels, buffer_size,
                                  sample_rate)
            : nullptr;

    input_buffers_.push_back(input_buffer);
    output_buffers_.push_back(output_buffer);
  }
  SetHandler(ScriptProcessorHandler::Create(
      *this, sample_rate, buffer_size, number_of_input_channels,
      number_of_output_channels, input_buffers_, output_buffers_));
}

static uint32_t ChooseBufferSize(uint32_t callback_buffer_size) {
  // Choose a buffer size based on the audio hardware buffer size. Arbitarily
  // make it a power of two that is 4 times greater than the hardware buffer
  // size.
  // TODO(crbug.com/855758): What is the best way to choose this?
  uint32_t buffer_size =
      1 << static_cast<uint32_t>(log2(4 * callback_buffer_size) + 0.5);

  if (buffer_size < 256)
    return 256;
  if (buffer_size > 16384)
    return 16384;

  return buffer_size;
}

ScriptProcessorNode* ScriptProcessorNode::Create(
    BaseAudioContext& context,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Default buffer size is 0 (let WebAudio choose) with 2 inputs and 2
  // outputs.
  return Create(context, 0, 2, 2, exception_state);
}

ScriptProcessorNode* ScriptProcessorNode::Create(
    BaseAudioContext& context,
    uint32_t requested_buffer_size,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Default is 2 inputs and 2 outputs.
  return Create(context, requested_buffer_size, 2, 2, exception_state);
}

ScriptProcessorNode* ScriptProcessorNode::Create(
    BaseAudioContext& context,
    uint32_t requested_buffer_size,
    uint32_t number_of_input_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Default is 2 outputs.
  return Create(context, requested_buffer_size, number_of_input_channels, 2,
                exception_state);
}

ScriptProcessorNode* ScriptProcessorNode::Create(
    BaseAudioContext& context,
    uint32_t requested_buffer_size,
    uint32_t number_of_input_channels,
    uint32_t number_of_output_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (number_of_input_channels == 0 && number_of_output_channels == 0) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "number of input channels and output channels cannot both be zero.");
    return nullptr;
  }

  if (number_of_input_channels > BaseAudioContext::MaxNumberOfChannels()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "number of input channels (" +
            String::Number(number_of_input_channels) + ") exceeds maximum (" +
            String::Number(BaseAudioContext::MaxNumberOfChannels()) + ").");
    return nullptr;
  }

  if (number_of_output_channels > BaseAudioContext::MaxNumberOfChannels()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "number of output channels (" +
            String::Number(number_of_output_channels) + ") exceeds maximum (" +
            String::Number(BaseAudioContext::MaxNumberOfChannels()) + ").");
    return nullptr;
  }

  // Sanitize user-supplied buffer size.
  uint32_t buffer_size;
  switch (requested_buffer_size) {
    case 0:
      // Choose an appropriate size.  For an AudioContext, we need to
      // choose an appropriate size based on the callback buffer size.
      // For OfflineAudioContext, there's no callback buffer size, so
      // just use the minimum valid buffer size.
      if (context.HasRealtimeConstraint()) {
        // TODO(crbug.com/854229): Due to the incompatible constructor between
        // AudioDestinationNode and RealtimeAudioDestinationNode, casting
        // directly from |destination()| is impossible. This is a temporary
        // workaround until the refactoring is completed.
        RealtimeAudioDestinationHandler& destination_handler =
            static_cast<RealtimeAudioDestinationHandler&>(
                context.destination()->GetAudioDestinationHandler());
        buffer_size =
            ChooseBufferSize(destination_handler.GetCallbackBufferSize());
      } else {
        buffer_size = 256;
      }
      break;
    case 256:
    case 512:
    case 1024:
    case 2048:
    case 4096:
    case 8192:
    case 16384:
      buffer_size = requested_buffer_size;
      break;
    default:
      exception_state.ThrowDOMException(
          DOMExceptionCode::kIndexSizeError,
          "buffer size (" + String::Number(requested_buffer_size) +
              ") must be 0 or a power of two between 256 and 16384.");
      return nullptr;
  }

  ScriptProcessorNode* node = MakeGarbageCollected<ScriptProcessorNode>(
      context, context.sampleRate(), buffer_size, number_of_input_channels,
      number_of_output_channels);

  if (!node)
    return nullptr;

  return node;
}

uint32_t ScriptProcessorNode::bufferSize() const {
  return static_cast<ScriptProcessorHandler&>(Handler()).BufferSize();
}

void ScriptProcessorNode::DispatchEvent(double playback_time,
                                        uint32_t double_buffer_index) {
  AudioBuffer* input_buffer = input_buffers_.at(double_buffer_index).Get();
  AudioBuffer* output_buffer = output_buffers_.at(double_buffer_index).Get();
  DCHECK(output_buffer);
  AudioNode::DispatchEvent(*AudioProcessingEvent::Create(
      input_buffer, output_buffer, playback_time));
}

bool ScriptProcessorNode::HasPendingActivity() const {
  // To prevent the node from leaking after the context is closed.
  if (context()->IsContextClosed())
    return false;

  // If |onaudioprocess| event handler is defined, the node should not be
  // GCed even if it is out of scope.
  if (HasEventListeners(event_type_names::kAudioprocess))
    return true;

  return false;
}

void ScriptProcessorNode::Trace(Visitor* visitor) {
  visitor->Trace(input_buffers_);
  visitor->Trace(output_buffers_);
  AudioNode::Trace(visitor);
}

void ScriptProcessorNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void ScriptProcessorNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
