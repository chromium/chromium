// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/cross_thread_source_location.h"

#include "base/check.h"
#include "base/not_fatal_until.h"

namespace blink {

CrossThreadSourceLocation CrossThreadSourceLocation::From(
    const SourceLocation* location) {
  if (location) {
    return CrossThreadSourceLocation(*location);
  }

  CHECK(false, base::NotFatalUntil::M150);
  return CrossThreadSourceLocation();
}

}  // namespace blink
