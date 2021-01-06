// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/module_request.h"

namespace blink {

String ModuleRequest::GetModuleTypeString() const {
  // Currently, Blink will get at most the single "type" assertion because
  // that's the only one requested from V8 (see
  // gin::IsoalteHolder::kSupportedImportAssertions). So this doesn't actually
  // have to be written as a loop at all unless more import assertions are
  // added. But, it's written as a loop anyway to be more future proof.
  DCHECK_LE(import_assertions.size(), 1U);
  for (const ImportAssertion& import_assertion : import_assertions) {
    if (import_assertion.key == "type") {
      DCHECK(!import_assertion.value.IsNull());
      return import_assertion.value;
    }
  }
  return String();
}

}  // namespace blink
