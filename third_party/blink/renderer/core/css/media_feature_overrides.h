// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_FEATURE_OVERRIDES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_FEATURE_OVERRIDES_H_

#include "third_party/blink/renderer/core/css/media_query_exp.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class CORE_EXPORT MediaFeatureOverrides {
 public:
  void SetOverride(const AtomicString& feature, const String& value_string);
  MediaQueryExpValue GetOverride(const AtomicString& feature) const {
    return overrides_.at(feature);
  }

 private:
  HashMap<AtomicString, MediaQueryExpValue> overrides_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_MEDIA_FEATURE_OVERRIDES_H_
