// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class MLContext;

class MODULES_EXPORT MLBuffer : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  MLBuffer(const MLBuffer&) = delete;
  MLBuffer& operator=(const MLBuffer&) = delete;

  ~MLBuffer() override;

  void Trace(Visitor* visitor) const override;

  // ml_buffer.idl
  void destroy();
  uint64_t size() const;

  const MLContext* context() const { return ml_context_.Get(); }

  // An MLBuffer should implement this method to read data from the MLBuffer.
  // Once the buffer is read, the resolver should be resolved with a copy of the
  // buffer data. Otherwise, the resolver should be rejected accordingly. The
  // caller must call `Promise()` on `resolver` before calling this method.
  virtual void ReadBufferImpl(
      ScriptPromiseResolver<DOMArrayBuffer>* resolver) = 0;

  // An MLBuffer should implement this method to write data into
  // the MLBuffer. If write was successful, the data will be stored
  // in the MLBuffer.
  virtual void WriteBufferImpl(base::span<const uint8_t> src_data,
                               ExceptionState& exception_state) = 0;

 protected:
  explicit MLBuffer(MLContext* context, uint64_t size);

  // An MLBuffer should implement this method to explicitly release
  // memory held by the platform buffer as soon as possible
  // instead of waiting for garbage collection.
  virtual void DestroyImpl() = 0;

  Member<MLContext> ml_context_;
  const uint64_t size_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_ML_BUFFER_H_
