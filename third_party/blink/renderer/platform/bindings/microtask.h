/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_MICROTASK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_MICROTASK_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

namespace blink {

// C++ calls into script contexts which are "owned" by blink (created in a
// process where WebKit.cpp initializes v8) must declare their type:
//
//   1. Calls into page/author script from a frame
//   2. Calls into page/author script from a worker
//   3. Calls into internal script (typically setup/teardown work)
//
// Debug-time checking of this is enforced via v8::MicrotasksScope.
//
// Calls of type (1) should generally go through ScriptController, as inspector
// instrumentation is needed. ScriptController allocates V8RecursionScope for
// you.
//
// Calls of type (2) should always stack-allocate a
// v8::MicrotasksScope(kRunMicrtoasks) in the same block as the call into
// script.
//
// Calls of type (3) should stack allocate a
// v8::MicrotasksScope(kDoNotRunMicrotasks) -- this skips work that is spec'd to
// happen at the end of the outer-most script stack frame of calls into page
// script:
// http://www.whatwg.org/specs/web-apps/current-work/#perform-a-microtask-checkpoint
class PLATFORM_EXPORT Microtask {
  STATIC_ONLY(Microtask);

 public:
  static void PerformCheckpoint(v8::Isolate*);

  // TODO(jochen): Make all microtasks pass in the ScriptState they want to be
  // executed in. Until then, all microtasks have to keep track of their
  // ScriptState themselves.
  static void EnqueueMicrotask(base::OnceClosure);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_MICROTASK_H_
