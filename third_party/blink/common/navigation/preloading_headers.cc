// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/navigation/preloading_headers.h"

namespace blink {

bool IsSecPurposeForPrefetch(std::optional<std::string> sec_purpose_header) {
  // All values used in Chromium and defined in `preloading_headers.h` start
  // with "prefetch".
  return sec_purpose_header && sec_purpose_header->starts_with("prefetch");
}

bool IsSecPurposeForPrerender(std::optional<std::string> sec_purpose_header) {
  return sec_purpose_header &&
         sec_purpose_header->starts_with("prefetch;prerender");
}

}  // namespace blink
