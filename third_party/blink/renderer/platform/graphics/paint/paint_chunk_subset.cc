// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_chunk_subset.h"

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

std::unique_ptr<JSONArray> PaintChunkSubset::ToJSON() const {
  auto json = std::make_unique<JSONArray>();
  for (auto it = begin(); it != end(); ++it) {
    StringBuilder sb;
    sb.Append("index=");
    sb.AppendNumber(it.IndexInPaintArtifact());
    sb.Append(" ");
    sb.Append(it->ToString(GetPaintArtifact()));
    json->PushString(sb.ToString());
  }
  return json;
}

std::ostream& operator<<(std::ostream& os, const PaintChunkSubset& subset) {
  return os << subset.ToJSON()->ToPrettyJSONString().Utf8();
}

}  // namespace blink
