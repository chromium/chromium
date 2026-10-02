// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/mixin_map.h"

#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

uint64_t MixinMap::AllocateMapIdentifier() {
  DCHECK(IsMainThread());
  static uint64_t next_map_identifier = 0;
  return next_map_identifier++;
}

bool MixinMap::HasSameContent(const MixinMap& other) const {
  if (media_query_result_flags != other.media_query_result_flags ||
      mixins.size() != other.mixins.size() ||
      media_query_set_results.size() != other.media_query_set_results.size()) {
    return false;
  }

  for (const auto& [name, mixin] : mixins) {
    auto it = other.mixins.find(name);
    if (it == other.mixins.end() || it->value != mixin) {
      return false;
    }
  }

  for (wtf_size_t i = 0; i < media_query_set_results.size(); ++i) {
    const MediaQuerySetResult& result = media_query_set_results[i];
    const MediaQuerySetResult& other_result = other.media_query_set_results[i];
    if (&result.MediaQueries() != &other_result.MediaQueries() ||
        result.Result() != other_result.Result()) {
      return false;
    }
  }
  return true;
}

void MixinMap::Merge(const MixinMap& other) {
  for (const auto& [key, value] : other.mixins) {
    mixins.Set(key, value);
  }
  media_query_result_flags.Add(other.media_query_result_flags);
  media_query_set_results.append_range(other.media_query_set_results);
}

}  // namespace blink
