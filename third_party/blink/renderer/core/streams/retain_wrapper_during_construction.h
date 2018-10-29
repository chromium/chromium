// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_RETAIN_WRAPPER_DURING_CONSTRUCTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_RETAIN_WRAPPER_DURING_CONSTRUCTION_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class ScriptState;
class ScriptWrappable;

// Objects which
// a) directly or indirectly own V8 objects held via TraceWrapperV8References,
//    and
// b) are constructed via the bindings
// need to call this function at the start of the constructor to ensure that a
// wrapper object exists for V8 to trace through. This ensures that the
// TraceWrapperV8Reference will not be collected even if a GC takes place during
// construction.
//
// This function posts a task containing a strong reference. The reference
// remains until the task is executed. This is slightly longer than necessary,
// but it shouldn't matter in practice.
//
// This function returns false if it fails. Usually this means the task runner
// is in the process of being destroyed. The caller must ensure that the
// reference is not used in this case.
//
// TODO(yhirano): Remove this once the unified GC is available.
bool CORE_EXPORT RetainWrapperDuringConstruction(ScriptWrappable*,
                                                 ScriptState*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_RETAIN_WRAPPER_DURING_CONSTRUCTION_H_
