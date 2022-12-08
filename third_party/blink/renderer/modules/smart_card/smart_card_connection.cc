// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_connection.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_disposition.h"

namespace blink {

SmartCardConnection::SmartCardConnection() = default;

ScriptPromise SmartCardConnection::disconnect(const V8SmartCardDisposition&) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise SmartCardConnection::status() {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

}  // namespace blink
