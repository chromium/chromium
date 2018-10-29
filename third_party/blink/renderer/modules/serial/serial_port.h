// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class ScriptState;
class SerialOptions;

class SerialPort final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ScriptValue in(ScriptState*);
  ScriptValue out(ScriptState*);

  ScriptPromise open(ScriptState*, const SerialOptions& options);
  ScriptPromise close(ScriptState*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SERIAL_SERIAL_PORT_H_
