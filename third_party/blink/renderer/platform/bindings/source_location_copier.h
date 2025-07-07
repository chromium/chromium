// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SOURCE_LOCATION_COPIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SOURCE_LOCATION_COPIER_H_

#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"

namespace blink {

template <>
struct CrossThreadCopier<CrossThreadSourceLocation> {
  STATIC_ONLY(CrossThreadCopier);

  static CrossThreadSourceLocation Copy(
      const CrossThreadSourceLocation& location) {
    return CrossThreadSourceLocation(
        location.url, location.function, location.line_number,
        location.column_number,
        location.stack_trace ? location.stack_trace->clone() : nullptr,
        location.script_id);
  }
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_SOURCE_LOCATION_COPIER_H_
