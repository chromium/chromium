// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_ELEMENT_LOCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_ELEMENT_LOCATOR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/element_locator.pb.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Element;

namespace element_locator {

// Attempt to generate an `ElementLocator` that specifies the relative position
// of the `element` within its document.
CORE_EXPORT absl::optional<ElementLocator> OfElement(Element* element);

// Generate a string representation of the given `ElementLocator`.
// Intended for testing and debugging purposes.
// Note: Since we are using the MessageLite runtime, TextFormat is not
//       available, so we need something on our own.
CORE_EXPORT String ToString(const ElementLocator&);

}  // namespace element_locator

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LCP_CRITICAL_PATH_PREDICTOR_LCP_CRITICAL_PATH_PREDICTOR_H_
