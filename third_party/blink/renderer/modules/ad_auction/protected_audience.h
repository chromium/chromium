// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_PROTECTED_AUDIENCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_PROTECTED_AUDIENCE_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptState;

class ProtectedAudience : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ProtectedAudience();
  static ScriptValue queryFeatureSupport(ScriptState* script_state,
                                         const String& feature_name);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AD_AUCTION_PROTECTED_AUDIENCE_H_
