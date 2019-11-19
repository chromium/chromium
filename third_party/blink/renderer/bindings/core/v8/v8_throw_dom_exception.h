// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_THROW_DOM_EXCEPTION_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_THROW_DOM_EXCEPTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/v8_private_property.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

// Provides utility functions to create and/or throw DOM Exceptions.
class CORE_EXPORT V8ThrowDOMException {
  STATIC_ONLY(V8ThrowDOMException);

 public:
  // Per-process initializer. Must be called in CoreInitializer.
  static void Init();

  // Creates and returns a DOMException object, or returns an empty handle if
  // the isolate is being terminated. Unlike the DOMException constructor,
  // this function associates the stacktrace with the returned object.
  //
  // |unsanitized_message| should be specified iff SecurityError.
  static v8::Local<v8::Value> CreateOrEmpty(
      v8::Isolate*,
      DOMExceptionCode,
      const String& sanitized_message,
      const String& unsanitized_message = String());
};

extern const V8PrivateProperty::SymbolKey kPrivatePropertyDOMExceptionError;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_THROW_DOM_EXCEPTION_H_
