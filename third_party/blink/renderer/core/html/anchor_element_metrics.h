// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_H_

#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class Document;
class HTMLAnchorElement;

// Helper function that returns the root document the anchor element is in.
Document* GetRootDocument(const HTMLAnchorElement& anchor);

uint32_t AnchorElementId(const HTMLAnchorElement& element);

// Exported for testing only.
CORE_EXPORT
mojom::blink::AnchorElementMetricsPtr CreateAnchorElementMetrics(
    const HTMLAnchorElement&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_METRICS_H_
