// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_DEFAULT_CONTROLLER_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_DEFAULT_CONTROLLER_INTERFACE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;

// Thin wrapper for TransformStreamDefaultController object. This abstracts away
// the differences between the JavaScript and C++ implementations.
// The API mimics the JavaScript API
// https://streams.spec.whatwg.org/#ts-default-controller-class but unneeded
// parts have not been implemented.
class CORE_EXPORT TransformStreamDefaultControllerInterface {
  STACK_ALLOCATED();

 public:
  TransformStreamDefaultControllerInterface() = default;
  virtual ~TransformStreamDefaultControllerInterface() = default;

  virtual void Enqueue(v8::Local<v8::Value> chunk, ExceptionState&) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TransformStreamDefaultControllerInterface);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TRANSFORM_STREAM_DEFAULT_CONTROLLER_INTERFACE_H_
