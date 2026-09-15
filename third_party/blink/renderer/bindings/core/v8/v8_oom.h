// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_OOM_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_OOM_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "v8/include/v8-callbacks.h"

namespace blink {

// Callback function invoked when V8 encounters an Out-Of-Memory (OOM) error.
// Records crash keys and terminates the process with an OOM crash.
CORE_EXPORT void ReportV8OOMError(const char* location,
                                  const v8::OOMDetails& details);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_OOM_H_
