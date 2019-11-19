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

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_import_step.h"

#include <stdio.h>
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_dispatcher.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_sync_microtask_queue.h"
#include "third_party/blink/renderer/core/html/imports/html_import_child.h"
#include "third_party/blink/renderer/core/html/imports/html_import_loader.h"

namespace blink {

V0CustomElementMicrotaskImportStep::V0CustomElementMicrotaskImportStep(
    HTMLImportChild* import)
    : import_(import), queue_(import->Loader()->MicrotaskQueue()) {}

V0CustomElementMicrotaskImportStep::~V0CustomElementMicrotaskImportStep() =
    default;

void V0CustomElementMicrotaskImportStep::Invalidate() {
  queue_ = MakeGarbageCollected<V0CustomElementSyncMicrotaskQueue>();
  import_.Clear();
}

bool V0CustomElementMicrotaskImportStep::ShouldWaitForImport() const {
  return import_ && !import_->Loader()->IsDone();
}

void V0CustomElementMicrotaskImportStep::DidUpgradeAllCustomElements() {
  DCHECK(queue_);
  if (import_)
    import_->DidFinishUpgradingCustomElements();
}

V0CustomElementMicrotaskStep::Result
V0CustomElementMicrotaskImportStep::Process() {
  queue_->Dispatch();
  if (!queue_->IsEmpty() || ShouldWaitForImport())
    return kProcessing;

  DidUpgradeAllCustomElements();
  return kFinishedProcessing;
}

void V0CustomElementMicrotaskImportStep::Trace(Visitor* visitor) {
  visitor->Trace(import_);
  visitor->Trace(queue_);
  V0CustomElementMicrotaskStep::Trace(visitor);
}

#if !defined(NDEBUG)
void V0CustomElementMicrotaskImportStep::Show(unsigned indent) {
  fprintf(stderr, "%*sImport(wait=%d sync=%d, url=%s)\n", indent, "",
          ShouldWaitForImport(), import_ && import_->IsSync(),
          import_ ? import_->Url().GetString().Utf8().c_str() : "null");
  queue_->Show(indent + 1);
}
#endif

}  // namespace blink
