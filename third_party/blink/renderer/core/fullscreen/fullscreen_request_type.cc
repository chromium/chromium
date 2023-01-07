// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fullscreen/fullscreen_request_type.h"

#if DCHECK_IS_ON()
#include <sstream>
#endif

namespace blink {

#if DCHECK_IS_ON()
std::string FullscreenRequestTypeToDebugString(FullscreenRequestType req) {
  std::stringstream result;
  result << (req & FullscreenRequestType::kPrefixed ? "Prefixed"
                                                    : "Unprefixed");
  if (req & FullscreenRequestType::kForCrossProcessDescendant)
    result << "|ForCrossProcessDescendant";
  if (req & FullscreenRequestType::kForXrOverlay)
    result << "|ForXrOverlay";
  if (req & FullscreenRequestType::kForXrArWithCamera)
    result << "|ForXrArWithCamera";
  return result.str();
}
#endif

}  // namespace blink
