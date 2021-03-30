// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CONVERSION_MEASUREMENT_PARSING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CONVERSION_MEASUREMENT_PARSING_H_

#include <stdint.h>
#include <memory>

#include "base/optional.h"
#include "third_party/blink/public/platform/web_impression.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExecutionContext;
class HTMLAnchorElement;
class ImpressionParams;

// Returns the WebImpression struct with all data declared by impression
// related attributes on |element|. If the impression attributes do not contain
// allowed values, base::nullopt is returned.
base::Optional<WebImpression> GetImpressionForAnchor(
    HTMLAnchorElement* element);

// Same as GetImpressionForAnchor(), but gets an impression specified by an
// ImpressionParams dictionary associated with a window.open call.
base::Optional<WebImpression> GetImpressionForParams(
    ExecutionContext* execution_context,
    const ImpressionParams* params);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CONVERSION_MEASUREMENT_PARSING_H_
