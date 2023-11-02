// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/module_request.h"

namespace blink {

String ModuleRequest::GetModuleTypeString() const {
  for (const ImportAssertion& import_assertion : import_assertions) {
    if (import_assertion.key == "type") {
      DCHECK(!import_assertion.value.IsNull());
      return import_assertion.value;
    }
  }
  return String();
}

}  // namespace blink
