// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"

#include "third_party/blink/renderer/platform/wtf/text/format.h"

namespace blink {

std::unique_ptr<JSONArray> PaintChunkSubset::ToJSON() const {
  auto json = std::make_unique<JSONArray>();
  for (auto it = begin(); it != end(); ++it) {
    json->PushString(Format("index={} {}", it.IndexInPaintArtifact(),
                            it->ToString(GetPaintArtifact())));
  }
  return json;
}

std::ostream& operator<<(std::ostream& os, const PaintChunkSubset& subset) {
  return os << subset.ToJSON()->ToPrettyJSONString().Utf8();
}

}  // namespace blink
