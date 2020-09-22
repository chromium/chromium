// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MODULE_REQUEST_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MODULE_REQUEST_H_

#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct ModuleRequest {
  String specifier;
  TextPosition position;
  ModuleRequest(const String& specifier, const TextPosition& position)
      : specifier(specifier), position(position) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MODULE_REQUEST_H_
