// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_STREAM_BUFFER_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_STREAM_BUFFER_UNDERLYING_SOURCE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"

namespace blink {

class BodyStreamBuffer;
class ScriptState;
class ScriptPromise;

class CORE_EXPORT BodyStreamBufferUnderlyingSource final
    : public UnderlyingSourceBase {
 public:
  BodyStreamBufferUnderlyingSource(ScriptState* script_state,
                                   BodyStreamBuffer* body_buffer)
      : UnderlyingSourceBase(script_state),
        script_state_(script_state),
        body_stream_buffer_(body_buffer) {}

  ScriptPromise pull(ScriptState*) override;
  ScriptPromise Cancel(ScriptState*,
                       ScriptValue reason,
                       ExceptionState&) override;

  ReadableStreamDefaultControllerWithScriptScope* Controller() const {
    return UnderlyingSourceBase::Controller();
  }

  void Trace(Visitor*) const override;

 private:
  Member<ScriptState> script_state_;
  Member<BodyStreamBuffer> body_stream_buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_BODY_STREAM_BUFFER_UNDERLYING_SOURCE_H_
