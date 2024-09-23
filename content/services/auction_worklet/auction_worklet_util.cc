// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/auction_worklet_util.h"

#include <string>

#include "base/types/optional_ref.h"
#include "gin/dictionary.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "v8/include/v8-isolate.h"

namespace auction_worklet {

bool CanSetAdSize(base::optional_ref<const blink::AdSize> ad_size) {
  return ad_size.has_value() && blink::IsValidAdSize(ad_size.value());
}

bool MaybeSetSizeMember(v8::Isolate* isolate,
                        gin::Dictionary& top_level_dict,
                        const std::string& member,
                        const blink::AdSize& ad_size) {
  gin::Dictionary size_dict = gin::Dictionary::CreateEmpty(isolate);
  return size_dict.Set("width", blink::ConvertAdDimensionToString(
                                    ad_size.width, ad_size.width_units)) &&
         size_dict.Set("height", blink::ConvertAdDimensionToString(
                                     ad_size.height, ad_size.height_units)) &&
         top_level_dict.Set(member, size_dict);
}

}  // namespace auction_worklet
