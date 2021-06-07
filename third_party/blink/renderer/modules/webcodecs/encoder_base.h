// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODER_BASE_H_

#include <memory>

#include "media/base/media_log.h"
#include "media/base/status.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_codec_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webcodecs_error_callback.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_logger.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_trace_names.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ExceptionState;
enum class DOMExceptionCode;

template <typename Traits>
class MODULES_EXPORT EncoderBase
    : public ScriptWrappable,
      public ActiveScriptWrappable<EncoderBase<Traits>>,
      public ExecutionContextLifecycleObserver {
 public:
  using InitType = typename Traits::Init;
  using ConfigType = typename Traits::Config;
  using InternalConfigType = typename Traits::InternalConfig;
  using InputType = typename Traits::Input;
  using EncodeOptionsType = typename Traits::EncodeOptions;
  using OutputChunkType = typename Traits::OutputChunk;
  using OutputCallbackType = typename Traits::OutputCallback;
  using MediaEncoderType = typename Traits::MediaEncoder;

  static const CodecTraceNames* GetTraceNames();

  EncoderBase(ScriptState*, const InitType*, ExceptionState&);
  ~EncoderBase() override;

  // *_encoder.idl implementation.
  int32_t encodeQueueSize() { return requested_encodes_; }

  void configure(const ConfigType*, ExceptionState&);

  void encode(InputType* input,
              const EncodeOptionsType* opts,
              ExceptionState& exception_state);

  ScriptPromise flush(ExceptionState&);

  void reset(ExceptionState&);

  void close(ExceptionState&);

  String state() { return state_; }

  // ExecutionContextLifecycleObserver override.
  void ContextDestroyed() override;

  // ScriptWrappable override.
  bool HasPendingActivity() const override;

  // GarbageCollected override.
  void Trace(Visitor*) const override;

 protected:
  struct Request final : public GarbageCollected<Request> {
    enum class Type {
      // Configure an encoder from scratch, possibly replacing the existing one.
      kConfigure,
      // Adjust options in the already configured encoder.
      kReconfigure,
      kEncode,
      kFlush,
    };

    void Trace(Visitor*) const;

    // Starts an async trace event.
    void StartTracing();

    // Starts an async encode trace.
    void StartTracingVideoEncode(bool is_keyframe);

    // Ends the async trace event associated with |this|.
    void EndTracing(bool aborted = false);

    // Get a trace event name from DecoderTemplate::GetTraceNames() and |type|.
    const char* TraceNameFromType();

    Type type;
    // Current value of EncoderBase.reset_count_ when request was created.
    uint32_t reset_count = 0;
    Member<InputType> input;                     // used by kEncode
    Member<const EncodeOptionsType> encodeOpts;  // used by kEncode
    Member<ScriptPromiseResolver> resolver;      // used by kFlush

#if DCHECK_IS_ON()
    // Tracks the state of tracing for debug purposes.
    bool is_tracing;
#endif
  };

  virtual void HandleError(DOMException* ex);
  virtual void EnqueueRequest(Request* request);
  virtual void ProcessRequests();
  virtual void ProcessEncode(Request* request) = 0;
  virtual void ProcessConfigure(Request* request) = 0;
  virtual void ProcessReconfigure(Request* request) = 0;
  virtual void ProcessFlush(Request* request);
  virtual void ResetInternal();

  virtual bool CanReconfigure(InternalConfigType& original_config,
                              InternalConfigType& new_config) = 0;
  virtual InternalConfigType* ParseConfig(const ConfigType*,
                                          ExceptionState&) = 0;
  virtual bool VerifyCodecSupport(InternalConfigType*, ExceptionState&) = 0;

  void TraceQueueSizes() const;

  std::unique_ptr<CodecLogger> logger_;

  std::unique_ptr<MediaEncoderType> media_encoder_;

  V8CodecState state_;

  Member<InternalConfigType> active_config_;
  Member<ScriptState> script_state_;
  Member<OutputCallbackType> output_callback_;
  Member<V8WebCodecsErrorCallback> error_callback_;
  HeapDeque<Member<Request>> requests_;
  int32_t requested_encodes_ = 0;

  // How many times reset() was called on the encoder. It's used to decide
  // when a callback needs to be dismissed because reset() was called between
  // an operation and its callback.
  uint32_t reset_count_ = 0;

  // Some kConfigure and kFlush requests can't be executed in parallel with
  // kEncode. This flag stops processing of new requests in the requests_ queue
  // till the current requests are finished.
  bool stall_request_processing_ = false;

  bool first_output_after_configure_ = true;

  // Used to differentiate Encoders' counters during tracing.
  int trace_counter_id_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODER_BASE_H_
