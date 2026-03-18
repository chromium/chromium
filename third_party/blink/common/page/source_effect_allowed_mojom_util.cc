// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/source_effect_allowed_mojom_util.h"

namespace blink {

blink::mojom::SourceEffectAllowed SourceEffectAllowedFromString(
    std::string_view source_effect_allowed) {
  if (source_effect_allowed == "none") {
    return blink::mojom::SourceEffectAllowed::kNone;
  }
  if (source_effect_allowed == "copy") {
    return blink::mojom::SourceEffectAllowed::kCopy;
  }
  if (source_effect_allowed == "copyLink") {
    return blink::mojom::SourceEffectAllowed::kCopyLink;
  }
  if (source_effect_allowed == "copyMove") {
    return blink::mojom::SourceEffectAllowed::kCopyMove;
  }
  if (source_effect_allowed == "link") {
    return blink::mojom::SourceEffectAllowed::kLink;
  }
  if (source_effect_allowed == "linkMove") {
    return blink::mojom::SourceEffectAllowed::kLinkMove;
  }
  if (source_effect_allowed == "move") {
    return blink::mojom::SourceEffectAllowed::kMove;
  }
  if (source_effect_allowed == "all") {
    return blink::mojom::SourceEffectAllowed::kAll;
  }
  return blink::mojom::SourceEffectAllowed::kUninitialized;
}

std::string_view SourceEffectAllowedToString(
    blink::mojom::SourceEffectAllowed source_effect_allowed) {
  switch (source_effect_allowed) {
    case blink::mojom::SourceEffectAllowed::kNone:
      return "none";
    case blink::mojom::SourceEffectAllowed::kCopy:
      return "copy";
    case blink::mojom::SourceEffectAllowed::kCopyLink:
      return "copyLink";
    case blink::mojom::SourceEffectAllowed::kCopyMove:
      return "copyMove";
    case blink::mojom::SourceEffectAllowed::kLink:
      return "link";
    case blink::mojom::SourceEffectAllowed::kLinkMove:
      return "linkMove";
    case blink::mojom::SourceEffectAllowed::kMove:
      return "move";
    case blink::mojom::SourceEffectAllowed::kAll:
      return "all";
    case blink::mojom::SourceEffectAllowed::kUninitialized:
      return "uninitialized";
  }
  return "uninitialized";
}

}  // namespace blink
