// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CRASH_REPORT_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CRASH_REPORT_STORAGE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

namespace blink {

class ExceptionState;
class LocalDOMWindow;

class CORE_EXPORT CrashReportStorage final : public ScriptWrappable,
                                             public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit CrashReportStorage(LocalDOMWindow& window);

  ScriptPromise<IDLUndefined> initialize(ScriptState* script_state,
                                         uint64_t length,
                                         ExceptionState&);

  void set(const String&, const String&, ExceptionState&);

  void remove(const String&, ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  void OnCreateCrashReportStorage(ScriptPromiseResolver<IDLUndefined>* resolver,
                                  uint64_t length);
  // This method can throw exceptions, if, for example, the developer attempts
  // to store a key/value combination that is too large. When the write fails
  // and it throws an exception, this method returns false so that the caller
  // can take action, if needed.
  bool CheckSizeAndWriteKey(const String& key,
                            const String& value,
                            ExceptionState& exception_state);

  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  // Ths member is a one-way boolean; it starts as false, and only gets set to
  // true in `OnCreateCrashReportStorage()`. It is used to prevent `set()` and
  // `remove()` from being used until the Promise returned by `initialize()` has
  // been resolved. This is not important for the current Chromium
  // implementation of this API, since `set()` and `remove()` are technically
  // valid to use immediately, as an implementation detail, but the
  // specification requires this gap to allow for implementations to
  // asynchronously initialize arbitrary backing memory mechanisms for this API.
  // Chromium may take advantage of this with shared memory, as is being
  // explored in https://crrev.com/c/6788146.
  bool initialization_complete_ = false;
  // This member is set a single time, exactly once, when
  // `initialization_complete_` is set. It tracks the requested length, so that
  // we can compare the size of `storage_` with the requested length in each
  // `set()` call.
  uint64_t length_ = 0;

  HashMap<String, String> storage_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CRASH_REPORT_STORAGE_H_
