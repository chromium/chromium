// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/report_body.h"

namespace blink {

ScriptValue ReportBody::toJSON(ScriptState* script_state) const {
  V8ObjectBuilder result(script_state);
  BuildJSONValue(result);
  return result.GetScriptValue();
}

}  // namespace blink
