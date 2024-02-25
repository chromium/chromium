// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_v8_context_snapshot.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_context_snapshot.h"
#include "v8/include/v8.h"

namespace blink {

v8::StartupData WebV8ContextSnapshot::TakeSnapshot(v8::Isolate* isolate) {
  return V8ContextSnapshot::TakeSnapshot(isolate);
}

}  // namespace blink
