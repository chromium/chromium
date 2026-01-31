// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CRASH_REPORT_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CRASH_REPORT_CONTEXT_H_

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;

class CORE_EXPORT CrashReportContext final : public ScriptWrappable,
                                             public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit CrashReportContext(LocalDOMWindow& window);

  ScriptPromise<IDLUndefined> initialize(ScriptState* script_state,
                                         uint64_t length,
                                         ExceptionState&);

  void set(const String& key, const String& value, ExceptionState&);
  void deleteKey(const String& key, ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  void OnCreateCrashReportContext(ScriptPromiseResolver<IDLUndefined>* resolver,
                                  base::UnsafeSharedMemoryRegion region);
  // This method can throw exceptions, if, for example, the developer attempts
  // to store a key/value combination that is too large. When the write fails
  // and it throws an exception, this method returns false so that the caller
  // can take action, if needed.
  bool WriteToSharedMemory(ExceptionState& exception_state);

  HashMap<String, String> storage_;
  base::WritableSharedMemoryMapping shm_mapping_;
  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CRASH_REPORT_CONTEXT_H_
