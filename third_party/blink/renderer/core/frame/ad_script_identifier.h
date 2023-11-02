// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_SCRIPT_IDENTIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_SCRIPT_IDENTIFIER_H_

#include "v8/include/v8-inspector.h"

namespace blink {

// Used to uniquely identify ad script on the stack.
struct AdScriptIdentifier {
  AdScriptIdentifier(const v8_inspector::V8DebuggerId& context_id, int id);

  // v8's debugging id for the v8::Context.
  v8_inspector::V8DebuggerId context_id;

  // The script's v8 identifier.
  int id;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_AD_SCRIPT_IDENTIFIER_H_
