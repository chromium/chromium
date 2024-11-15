// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/sanitizer/sanitizer_api.h"

namespace blink {

void SanitizerAPI::SanitizeSafeInternal(ContainerNode* element,
                                        SetHTMLOptions* options,
                                        ExceptionState& exception_state) {
  // Not implemented yet. This is split out into a subsequent CL.
}

void SanitizerAPI::SanitizeUnsafeInternal(ContainerNode* element,
                                          SetHTMLOptions* options,
                                          ExceptionState& exception_state) {
  // Not implemented yet. This is split out into a subsequent CL.
}

}  // namespace blink
