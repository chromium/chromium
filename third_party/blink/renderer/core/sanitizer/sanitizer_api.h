// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_API_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_API_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer.h"

// This file includes the entry points for the Sanitizer API.

namespace blink {

class ContainerNode;
class ExceptionState;
class FragmentParserOptions;
class StreamingSanitizer;

class CORE_EXPORT SanitizerAPI final {
 public:
  static void SanitizeInternal(Sanitizer::Mode mode,
                               const ContainerNode* context_element,
                               ContainerNode* root_element,
                               FragmentParserOptions options,
                               ExceptionState& exception_state);
  static StreamingSanitizer* CreateStreamingSanitizerInternal(
      FragmentParserOptions options,
      const ContainerNode* context,
      ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_API_H_
