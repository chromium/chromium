// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"

#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_manager.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

void ActiveScriptWrappableBase::RegisterActiveScriptWrappable(
    v8::Isolate* isolate) {
  V8PerIsolateData::From(isolate)->GetActiveScriptWrappableManager()->Add(this);
}

}  // namespace blink
