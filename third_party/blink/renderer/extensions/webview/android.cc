// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/webview/android.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

Android::Android() = default;

WebViewAndroid* Android::webview(ExecutionContext* execution_context) {
  return &WebViewAndroid::From(*execution_context);
}

void Android::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
