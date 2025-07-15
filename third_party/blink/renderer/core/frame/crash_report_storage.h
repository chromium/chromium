// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CRASH_REPORT_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CRASH_REPORT_STORAGE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

namespace blink {

class LocalDOMWindow;

class CORE_EXPORT CrashReportStorage final : public ScriptWrappable,
                                             public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit CrashReportStorage(LocalDOMWindow& window);

  void set(const String&, const String&, ExceptionState&);

  void remove(const String&, ExceptionState&);

  void Trace(Visitor*) const override;

 private:
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_CRASH_REPORT_STORAGE_H_
