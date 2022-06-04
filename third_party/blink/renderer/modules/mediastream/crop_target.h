// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CROP_TARGET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CROP_TARGET_H_

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class ExecutionContext;

class MODULES_EXPORT CropTarget final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ScriptPromise fromElement(ExecutionContext* execution_context,
                                   Element* element,
                                   ExceptionState& exception_state);

 private:
  CropTarget();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_CROP_TARGET_H_
