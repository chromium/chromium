// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_IDS_H_
#define PDF_PDF_INK_IDS_H_

#include <stddef.h>

#include <variant>

#include "base/functional/callback_forward.h"
#include "base/types/strong_alias.h"

// Defines various IDs used for PDF Ink Signatures. The IDs use
// base::StrongAlias to avoid type confusion.

namespace chrome_pdf {

// Identifies a unique font from the frontend.
using FontId = base::StrongAlias<class InkFontIdTag, int>;

// Identifies ink::PartitionedMesh objects.
using InkModeledShapeId = base::StrongAlias<class InkModeledShapeIdTag, size_t>;

// Identifies ink::Stroke objects.
using InkStrokeId = base::StrongAlias<class InkStrokeIdTag, size_t>;

// Identifies Ink text objects loaded from the PDF.
using InkLoadedTextId = base::StrongAlias<class InkLoadedTextIdTag, size_t>;

// Identifies newly created Ink text objects.
using InkTextId = base::StrongAlias<class InkTextIdTag, size_t>;

// Set of all IDs.
using IdType =
    std::variant<InkStrokeId, InkModeledShapeId, InkTextId, InkLoadedTextId>;

// Variant of only text IDs.
using TextId = std::variant<InkTextId, InkLoadedTextId>;

// A callback to generate a unique ID for Ink text objects.
using GenerateTextIdCallback = base::RepeatingCallback<InkTextId()>;

// Returns the underlying value of an IdType.
size_t GetIdTypeValue(const IdType& id);

// Compares IdTypes by their underlying values, breaking ties by comparing their
// index.
struct IdTypeComparator {
  bool operator()(const IdType& lhs, const IdType& rhs) const {
    const size_t lhs_value = GetIdTypeValue(lhs);
    const size_t rhs_value = GetIdTypeValue(rhs);
    if (lhs_value != rhs_value) {
      return lhs_value < rhs_value;
    }
    return lhs.index() < rhs.index();
  }
};

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_IDS_H_
