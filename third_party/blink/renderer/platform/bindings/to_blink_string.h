// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TO_BLINK_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TO_BLINK_STRING_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-forward.h"

namespace blink {

enum ExternalMode { kExternalize, kDoNotExternalize };

template <typename StringType>
PLATFORM_EXPORT StringType ToBlinkString(v8::Isolate* isolate,
                                         v8::Local<v8::String>,
                                         ExternalMode);

PLATFORM_EXPORT String ToBlinkString(int value);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TO_BLINK_STRING_H_
