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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_MICROTASK_IMPORT_STEP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_MICROTASK_IMPORT_STEP_H_

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_microtask_step.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class V0CustomElementSyncMicrotaskQueue;
class HTMLImportChild;

// Processes the Custom Elements in an HTML Import. This is a
// composite step which processes the Custom Elements created by
// parsing the import, and its sub-imports.
//
// This step blocks further Custom Element microtask processing if its
// import isn't "ready" (finished parsing and running script.)
class V0CustomElementMicrotaskImportStep final
    : public V0CustomElementMicrotaskStep {
 public:
  explicit V0CustomElementMicrotaskImportStep(HTMLImportChild*);
  ~V0CustomElementMicrotaskImportStep() override;

  // API for HTML Imports
  void Invalidate();
  void ImportDidFinishLoading();

  void Trace(Visitor*) override;

 private:
  void DidUpgradeAllCustomElements();
  bool ShouldWaitForImport() const;

  // V0CustomElementMicrotaskStep
  Result Process() final;

#if !defined(NDEBUG)
  void Show(unsigned indent) override;
#endif
  WeakMember<HTMLImportChild> import_;
  Member<V0CustomElementSyncMicrotaskQueue> queue_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_V0_CUSTOM_ELEMENT_MICROTASK_IMPORT_STEP_H_
