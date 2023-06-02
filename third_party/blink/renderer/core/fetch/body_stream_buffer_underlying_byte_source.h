// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_STREAM_BUFFER_UNDERLYING_BYTE_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_STREAM_BUFFER_UNDERLYING_BYTE_SOURCE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/streams/underlying_byte_source_base.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class BodyStreamBuffer;
class ExceptionState;
class ReadableByteStreamController;
class ScriptPromise;
class ScriptState;

class CORE_EXPORT BodyStreamBufferUnderlyingByteSource final
    : public UnderlyingByteSourceBase {
 public:
  explicit BodyStreamBufferUnderlyingByteSource(
      ScriptState* script_state,
      BodyStreamBuffer* body_stream_buffer)
      : script_state_(script_state), body_stream_buffer_(body_stream_buffer) {}

  ScriptPromise Pull(ReadableByteStreamController* controller,
                     ExceptionState&) override;

  ScriptPromise Cancel(ExceptionState&) override;
  ScriptPromise Cancel(v8::Local<v8::Value> reason, ExceptionState&) override;

  ScriptState* GetScriptState() override;

  void Trace(Visitor*) const override;

 private:
  const Member<ScriptState> script_state_;
  const Member<BodyStreamBuffer> body_stream_buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_STREAM_BUFFER_UNDERLYING_BYTE_SOURCE_H_
