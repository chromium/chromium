// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_MANAGER_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class ScriptState;
class ScriptValue;

class FontManager final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  FontManager() = default;
  ScriptValue query(ScriptState*, ExceptionState&);

  DISALLOW_COPY_AND_ASSIGN(FontManager);
  void Trace(blink::Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FONT_ACCESS_FONT_MANAGER_H_
