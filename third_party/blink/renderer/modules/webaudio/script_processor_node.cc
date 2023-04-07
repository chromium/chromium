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

#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable_creation_key.h"
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
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

bool IsAudioBufferDetached(AudioBuffer* buffer) {
  bool is_buffer_detached = false;
  for (unsigned channel = 0; channel < buffer->numberOfChannels(); ++channel) {
    if (buffer->getChannelData(channel)->buffer()->IsDetached()) {
      is_buffer_detached = true;
      break;
    }
  }

  return is_buffer_detached;
}

bool BufferTopologyMatches(AudioBuffer* buffer_1, AudioBuffer* buffer_2) {
  return (buffer_1->numberOfChannels() == buffer_2->numberOfChannels()) &&
         (buffer_1->length() == buffer_2->length()) &&
         (buffer_1->sampleRate() == buffer_2->sampleRate());
}

uint32_t ChooseBufferSize(uint32_t callback_buffer_size) {
  // Choose a buffer size based on the audio hardware buffer size. Arbitrarily
  // make it a power of two that is 4 times greater than the hardware buffer
  // size.
  // TODO(crbug.com/855758): What is the best way to choose this?
  uint32_t buffer_size =
      1 << static_cast<uint32_t>(log2(4 * callback_buffer_size) + 0.5);

  if (buffer_size < 256) {
    return 256;
  }
  if (buffer_size > 16384) {
    return 16384;
  }

  return buffer_size;
}

}  // namespace

ScriptProcessorNode::ScriptProcessorNode(BaseAudioContext& context,
                                         float sample_rate,
                                         uint32_t buffer_size,
                                         uint32_t number_of_input_channels,
                                         uint32_t number_of_output_channels)
    : AudioNode(context), ActiveScriptWrappable<ScriptProcessorNode>({}) {
  // Regardless of the allowed buffer sizes, we still need to process at the
  // granularity of the AudioNode.
  if (buffer_size < context.GetDeferredTaskHandler().RenderQuantumFrames()) {
    buffer_size = context.GetDeferredTaskHandler().RenderQuantumFrames();
  }

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

  external_input_buffer_ = AudioBuffer::Create(
      number_of_input_channels, buffer_size, sample_rate);
  external_output_buffer_ = AudioBuffer::Create(
      number_of_output_channels, buffer_size, sample_rate);

  SetHandler(ScriptProcessorHandler::Create(
      *this, sample_rate, buffer_size, number_of_input_channels,
      number_of_output_channels, input_buffers_, output_buffers_));
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
      // Choose an appropriate size.  For an AudioContext that is not closed, we
      // need to choose an appropriate size based on the callback buffer size.
      if (context.HasRealtimeConstraint() && !context.IsContextCleared()) {
        RealtimeAudioDestinationHandler& destination_handler =
            static_cast<RealtimeAudioDestinationHandler&>(
                context.destination()->GetAudioDestinationHandler());
        buffer_size =
            ChooseBufferSize(destination_handler.GetCallbackBufferSize());
      } else {
        // An OfflineAudioContext has no callback buffer size, so just use the
        // minimum.  If the realtime context is closed, we can't guarantee the
        // we can get the callback size, so use this same default.  (With the
        // context closed, there's not much you can do with this node anyway.)
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

  if (!node) {
    return nullptr;
  }

  return node;
}

uint32_t ScriptProcessorNode::bufferSize() const {
  return static_cast<ScriptProcessorHandler&>(Handler()).BufferSize();
}

void ScriptProcessorNode::DispatchEvent(double playback_time,
                                        uint32_t double_buffer_index) {
  DCHECK(IsMainThread());

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
               "ScriptProcessorNode::DispatchEvent");

  ScriptProcessorHandler& handler =
      static_cast<ScriptProcessorHandler&>(Handler());

  {
    base::AutoLock locker(handler.GetBufferLock());
    TRACE_EVENT1(
        TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
        "ScriptProcessorNode::DispatchEvent (copy input under lock)",
        "double_buffer_index", double_buffer_index);

    AudioBuffer* backing_input_buffer =
        input_buffers_.at(double_buffer_index).Get();

    // The backing buffer can be `nullptr`, when the number of input channels
    // is 0.
    if (backing_input_buffer) {
      // Also the author code might have transferred `external_input_buffer_` to
      // other threads or replaced it with a different AudioBuffer object. Then
      // re-create a new buffer instance.
      if (IsAudioBufferDetached(external_input_buffer_) ||
          !BufferTopologyMatches(backing_input_buffer,
                                external_input_buffer_)) {
        TRACE_EVENT0(
            TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
            "ScriptProcessorNode::DispatchEvent (create input AudioBuffer)");
        external_input_buffer_ = AudioBuffer::Create(
            backing_input_buffer->numberOfChannels(),
            backing_input_buffer->length(),
            backing_input_buffer->sampleRate());
      }

      for (unsigned channel = 0;
          channel < backing_input_buffer->numberOfChannels(); ++channel) {
        const float* source = static_cast<float*>(
            backing_input_buffer->getChannelData(channel)->buffer()->Data());
        float* destination = static_cast<float*>(
            external_input_buffer_->getChannelData(channel)->buffer()->Data());
        memcpy(destination, source,
               backing_input_buffer->length() * sizeof(float));
      }
    }
  }

  external_output_buffer_->Zero();

  AudioNode::DispatchEvent(*AudioProcessingEvent::Create(
      external_input_buffer_, external_output_buffer_, playback_time));

  {
    base::AutoLock locker(handler.GetBufferLock());
    TRACE_EVENT1(
        TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
        "ScriptProcessorNode::DispatchEvent (copy output under lock)",
        "double_buffer_index", double_buffer_index);

    AudioBuffer* backing_output_buffer =
        output_buffers_.at(double_buffer_index).Get();

    if (backing_output_buffer) {
      if (IsAudioBufferDetached(external_output_buffer_) ||
          !BufferTopologyMatches(backing_output_buffer,
                                 external_output_buffer_)) {
        TRACE_EVENT0(
            TRACE_DISABLED_BY_DEFAULT("webaudio.audionode"),
            "ScriptProcessorNode::DispatchEvent (create output AudioBuffer)");
        external_output_buffer_ = AudioBuffer::Create(
            backing_output_buffer->numberOfChannels(),
            backing_output_buffer->length(),
            backing_output_buffer->sampleRate());
      }

      for (unsigned channel = 0;
          channel < backing_output_buffer->numberOfChannels(); ++channel) {
        const float* source = static_cast<float*>(
            external_output_buffer_->getChannelData(channel)->buffer()->Data());
        float* destination = static_cast<float*>(
            backing_output_buffer->getChannelData(channel)->buffer()->Data());
        memcpy(destination, source,
               backing_output_buffer->length() * sizeof(float));
      }
    }
  }
}

bool ScriptProcessorNode::HasPendingActivity() const {
  // To prevent the node from leaking after the context is closed.
  if (context()->IsContextCleared()) {
    return false;
  }

  // If `.onaudioprocess` event handler is defined, the node should not be
  // GCed even if it is out of scope.
  if (HasEventListeners(event_type_names::kAudioprocess)) {
    return true;
  }

  return false;
}

void ScriptProcessorNode::Trace(Visitor* visitor) const {
  visitor->Trace(input_buffers_);
  visitor->Trace(output_buffers_);
  visitor->Trace(external_input_buffer_);
  visitor->Trace(external_output_buffer_);
  AudioNode::Trace(visitor);
}

void ScriptProcessorNode::ReportDidCreate() {
  GraphTracer().DidCreateAudioNode(this);
}

void ScriptProcessorNode::ReportWillBeDestroyed() {
  GraphTracer().WillDestroyAudioNode(this);
}

}  // namespace blink
