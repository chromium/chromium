// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/crash_report_storage.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {}  // namespace

CrashReportStorage::CrashReportStorage(LocalDOMWindow& window)
    : ExecutionContextClient(&window) {
  DCHECK(RuntimeEnabledFeatures::CrashReportingStorageAPIEnabled(
      GetExecutionContext()));
}

void CrashReportStorage::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

void CrashReportStorage::set(const String& key,
                             const String& value,
                             ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "Cannot use CrashReportStorage with a document that is not fully "
        "active.");
    return;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame->GetDocument());

  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().SetCrashReportStorageKey(key, value);
}

void CrashReportStorage::remove(const String& key,
                                ExceptionState& exception_state) {
  if (!DomWindow()) {
    exception_state.ThrowSecurityError(
        "Cannot use CrashReportStorage with a document that is not fully "
        "active.");
    return;
  }

  LocalFrame* frame = DomWindow()->GetFrame();
  DCHECK(frame->GetDocument());

  // Synchronous mojo call.
  frame->GetLocalFrameHostRemote().RemoveCrashReportStorageKey(key);
}

}  // namespace blink
