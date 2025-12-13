// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/mixin_map.h"

#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

void MixinMap::Merge(const MixinMap& other) {
  for (const auto& [key, value] : other.mixins) {
    mixins.Set(key, value);
  }
  media_query_result_flags.Add(other.media_query_result_flags);
  media_query_set_results.AppendVector(other.media_query_set_results);
}

}  // namespace blink
