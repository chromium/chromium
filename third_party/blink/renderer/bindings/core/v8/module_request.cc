// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/module_request.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

String ModuleRequest::GetModuleTypeString() const {
  for (const ImportAttribute& import_attribute : import_attributes) {
    if (import_attribute.key == "type") {
      DCHECK(!import_attribute.value.IsNull());
      return import_attribute.value;
    }
  }
  return String();
}

bool ModuleRequest::HasInvalidImportAttributeKey(String* invalid_key) const {
  if (!RuntimeEnabledFeatures::ImportAttributesDisallowUnknownKeysEnabled()) {
    return false;
  }

  for (const ImportAttribute& attr : import_attributes) {
    if (attr.key != "type") {
      *invalid_key = attr.key;
      return true;
    }
  }
  return false;
}

}  // namespace blink
