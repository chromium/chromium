// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ITERATOR_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;

class CORE_EXPORT Iterator : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  Iterator() = default;
  ~Iterator() override = default;

  virtual ScriptValue next(ScriptState*, ExceptionState&) = 0;
  virtual ScriptValue next(ScriptState*,
                           ScriptValue /* value */,
                           ExceptionState&) = 0;
  Iterator* GetIterator(ScriptState*, ExceptionState&) { return this; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ITERATOR_H_
