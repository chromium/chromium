// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_MODIFIER_METHOD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_MODIFIER_METHOD_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class MODULES_EXPORT SharedStorageModifierMethod : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SHARED_STORAGE_SHARED_STORAGE_MODIFIER_METHOD_H_
