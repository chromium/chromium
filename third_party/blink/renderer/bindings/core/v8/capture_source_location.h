// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CAPTURE_SOURCE_LOCATION_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CAPTURE_SOURCE_LOCATION_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"

namespace blink {

class ExecutionContext;

// Shortcut when location is unknown. Tries to capture call stack or parsing
// location if available.
CORE_EXPORT std::unique_ptr<SourceLocation> CaptureSourceLocation(
    ExecutionContext*);

// Shortcut when location is unknown. Tries to capture call stack or parsing
// location using message if available.
CORE_EXPORT std::unique_ptr<SourceLocation> CaptureSourceLocation(
    v8::Isolate* isolate,
    v8::Local<v8::Message> message,
    ExecutionContext* execution_context);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CAPTURE_SOURCE_LOCATION_H_
