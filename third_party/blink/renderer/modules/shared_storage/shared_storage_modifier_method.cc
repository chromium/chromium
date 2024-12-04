// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_modifier_method.h"

namespace blink {

void SharedStorageModifierMethod::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr
SharedStorageModifierMethod::TakeMojomMethod() {
  return std::move(method_with_options_);
}

network::mojom::blink::SharedStorageModifierMethodWithOptionsPtr
SharedStorageModifierMethod::CloneMojomMethod() {
  return method_with_options_.Clone();
}

}  // namespace blink
