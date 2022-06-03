// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_base.h"

#include "third_party/blink/renderer/platform/bindings/active_script_wrappable_manager.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"

namespace blink {

void ActiveScriptWrappableBase::ActiveScriptWrappableBaseConstructed() {
  DCHECK(ThreadState::Current());
  V8PerIsolateData::From(ThreadState::Current()->GetIsolate())
      ->GetActiveScriptWrappableManager()
      ->Add(this);
}

}  // namespace blink
