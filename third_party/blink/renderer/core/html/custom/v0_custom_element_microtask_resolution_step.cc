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

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_resolution_step.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/custom/v0_custom_element_registration_context.h"

namespace blink {

V0CustomElementMicrotaskResolutionStep::V0CustomElementMicrotaskResolutionStep(
    V0CustomElementRegistrationContext* context,
    Element* element,
    const V0CustomElementDescriptor& descriptor)
    : context_(context), element_(element), descriptor_(descriptor) {}

V0CustomElementMicrotaskResolutionStep::
    ~V0CustomElementMicrotaskResolutionStep() = default;

V0CustomElementMicrotaskStep::Result
V0CustomElementMicrotaskResolutionStep::Process() {
  context_->Resolve(element_.Get(), descriptor_);
  return V0CustomElementMicrotaskStep::kFinishedProcessing;
}

void V0CustomElementMicrotaskResolutionStep::Trace(Visitor* visitor) {
  visitor->Trace(context_);
  visitor->Trace(element_);
  V0CustomElementMicrotaskStep::Trace(visitor);
}

#if !defined(NDEBUG)
void V0CustomElementMicrotaskResolutionStep::Show(unsigned indent) {
  fprintf(stderr, "%*sResolution: ", indent, "");
  element_->OuterHTMLAsString().Show();
}
#endif

}  // namespace blink
