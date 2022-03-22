// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CONVERSION_MEASUREMENT_PARSING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CONVERSION_MEASUREMENT_PARSING_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {

class HTMLAnchorElement;

struct WebImpression;

// Returns the WebImpression struct with all data declared by impression
// related attributes on |element|. If the impression attributes do not contain
// allowed values, absl::nullopt is returned.
absl::optional<WebImpression> GetImpressionForAnchor(
    HTMLAnchorElement* element);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CONVERSION_MEASUREMENT_PARSING_H_
