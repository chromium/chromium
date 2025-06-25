// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_UTIL_H_
#define CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_UTIL_H_

#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"

namespace content {

// Distinguishes the given speculation eagerness should be treated as
// "immediate" eagerness. Note that This means that non-`kImmediate` eagerness
// may behaves as `kImmediate`.
inline constexpr bool IsImmediateSpeculationEagerness(
    blink::mojom::SpeculationEagerness eagerness) {
  switch (eagerness) {
    // Currently, `kEager` behaves the same as `kImmediate`,
    // but it will be changed in the near future as ongoing improvements on
    // `kEager` trigger strategies.
    // TODO(crbug.com/40287486): Update this according to the latest situation.
    case blink::mojom::SpeculationEagerness::kImmediate:
    case blink::mojom::SpeculationEagerness::kEager:
      return true;
    case blink::mojom::SpeculationEagerness::kModerate:
    case blink::mojom::SpeculationEagerness::kConservative:
      return false;
  }
}

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_SPECULATION_RULES_SPECULATION_RULES_UTIL_H_
