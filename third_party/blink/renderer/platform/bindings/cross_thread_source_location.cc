// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/cross_thread_source_location.h"

#include "base/check_deref.h"

namespace blink {

CrossThreadSourceLocation CrossThreadSourceLocation::From(
    const SourceLocation* location) {
  return CrossThreadSourceLocation(CHECK_DEREF(location));
}

}  // namespace blink
