// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_PROCESSOR_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_processor_error_details.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

class AudioBus;
class AudioWorkletGlobalScope;
class AudioWorkletProcessorDefinition;
class MessagePort;
class ExecutionContext;
class V8BlinkAudioWorkletProcessCallback;

// AudioWorkletProcessor class represents the active instance created from
// AudioWorkletProcessorDefinition. AudioWorkletNodeHandler invokes `.process()`
// method in this object upon graph rendering.
//
// This is constructed and destroyed on a worker thread, and all methods also
// must be called on the worker thread.
class MODULES_EXPORT AudioWorkletProcessor : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // This static factory should be called after an instance of
  // AudioWorkletNode gets created by user-supplied JS code in the main
  // thread. This factory must not be called by user in
  // AudioWorkletGlobalScope.
  static AudioWorkletProcessor* Create(ExecutionContext*, ExceptionState&);

  AudioWorkletProcessor(AudioWorkletGlobalScope*,
                        const String& name,
                        MessagePort*);
  ~AudioWorkletProcessor() override;

  void Trace(Visitor*) const override;

  // IDL
  MessagePort* port() const;

  // `AudioWorkletHandler` invokes this method to process audio.
  bool Process(const Vector<scoped_refptr<AudioBus>>& inputs,
               Vector<scoped_refptr<AudioBus>>& outputs,
               const HashMap<String, base::span<const float>>& param_value_map);

  bool hasErrorOccurred() const;
  const String& Name() const { return name_; }
  const AudioWorkletProcessorErrorDetails& GetErrorDetails() const;

  // Builds error details from the exception captured in `try_catch`. Falls
  // back to `fallback_message` with no source location when nothing was
  // caught or V8 provides no message. Must be called on the isolate's thread
  // while `try_catch` is still live. Never returns null strings so the result
  // can be copied across threads by AudioWorkletProcessorErrorDetails.
  static AudioWorkletProcessorErrorDetails ErrorDetailsFromTryCatch(
      v8::Isolate* isolate,
      v8::Local<v8::Context> context,
      const v8::TryCatch& try_catch,
      AudioWorkletProcessorErrorState error_state,
      const String& fallback_message);

 private:
  void SetErrorDetails(const AudioWorkletProcessorErrorDetails& error_details);

  Member<AudioWorkletGlobalScope> global_scope_;
  Member<MessagePort> processor_port_;
  Member<V8BlinkAudioWorkletProcessCallback> cached_process_callback_;

  const String name_;

  TraceWrapperV8Reference<v8::Array> inputs_;
  TraceWrapperV8Reference<v8::Array> outputs_;
  TraceWrapperV8Reference<v8::Object> params_;

  HeapVector<HeapVector<TraceWrapperV8Reference<v8::ArrayBuffer>>>
      input_array_buffers_;
  HeapVector<HeapVector<TraceWrapperV8Reference<v8::ArrayBuffer>>>
      output_array_buffers_;

  AudioWorkletProcessorErrorDetails error_details_;

  const bool is_denormal_enabler_enabled_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_PROCESSOR_H_
