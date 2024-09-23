// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_UTIL_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_UTIL_H_

#include <string>

#include "base/types/optional_ref.h"
#include "content/common/content_export.h"
#include "gin/dictionary.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"

namespace v8 {
class Isolate;
}

namespace auction_worklet {

// Returns true if the ad size is valid to set as a member to a dictionary.
CONTENT_EXPORT bool CanSetAdSize(
    base::optional_ref<const blink::AdSize> ad_size);

// Try set an ad size member in `top_level_dict` like this:
//
// "top_level_dict": {
//   "member": {
//     "width": "100sw",
//     "height": "50px",
//   }
// }
//
// Returns false when the set fails. The `top_level_dict` stays intact.
CONTENT_EXPORT bool MaybeSetSizeMember(v8::Isolate* isolate,
                                       gin::Dictionary& top_level_dict,
                                       const std::string& member,
                                       const blink::AdSize& ad_size);

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_WORKLET_UTIL_H_
