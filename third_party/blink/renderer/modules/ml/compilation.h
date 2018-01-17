// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Compilation_h
#define Compilation_h

#include "bindings/core/v8/ScriptPromise.h"
#include "platform/bindings/ScriptWrappable.h"

namespace blink {

class ExceptionState;
class Execution;

class Compilation final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
 public:
  Compilation();
  ~Compilation() override;

  void setPreference(uint32_t preference, ExceptionState& state);
  ScriptPromise finish(ScriptState*);
  Execution* createExecution();

  void Trace(blink::Visitor*);
};

}  // namespace blink

#endif  // Compilation_h