// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/layout/exclusions/exclusion_area.h"

#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

void ExclusionShapeData::Trace(Visitor* visitor) const {
  visitor->Trace(layout_box);
}

bool ExclusionArea::operator==(const ExclusionArea& other) const {
  return type == other.type && kind == other.kind && rect == other.rect &&
         is_hidden_for_paint == other.is_hidden_for_paint &&
         shape_data == other.shape_data;
}

namespace {

struct PrintableEFloat {
  explicit PrintableEFloat(EFloat value) : value(value) {}
  EFloat value;
};

std::ostream& operator<<(std::ostream& os, const PrintableEFloat& printable) {
  const char* kStrings[] = {
      "kNone", "kLeft", "kRight", "kInlineStart", "kInlineEnd",
  };
  const unsigned index = static_cast<unsigned>(printable.value);
  if (index >= std::size(kStrings))
    return os << "EFloat::" << index;
  return os << "EFloat::" << kStrings[index];
}

struct PrintableKind {
  explicit PrintableKind(ExclusionArea::Kind value) : value(value) {}
  ExclusionArea::Kind value;
};

std::ostream& operator<<(std::ostream& os, const PrintableKind& printable) {
  const char* kStrings[] = {
      "kFloat",
      "kInitialLetterBox",
  };
  const unsigned index = static_cast<unsigned>(printable.value);
  if (index >= std::size(kStrings))
    return os << "Kind::" << index;
  return os << kStrings[index];
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const ExclusionArea& exclusion) {
  return os << "ExclusionArea(" << PrintableKind(exclusion.kind) << ", "
            << PrintableEFloat(exclusion.type) << ", " << exclusion.rect << ")";
}

std::ostream& operator<<(std::ostream& os, const ExclusionArea* exclusion) {
  if (!exclusion)
    return os << "(nullptr)";
  return os << *exclusion;
}

}  // namespace blink
