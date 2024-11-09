// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_IDS_H_
#define PDF_PDF_INK_IDS_H_

#include <stddef.h>

#include "base/types/strong_alias.h"

// Defines various IDs used for PDF Ink Signatures. The IDs use
// base::StrongAlias to avoid type confusion.

namespace chrome_pdf {

// Identifies ink::ModeledShape objects.
using InkModeledShapeId = base::StrongAlias<class InkModeledShapeIdTag, size_t>;

// Identifies ink::Stroke objects.
using InkStrokeId = base::StrongAlias<class InkStrokeIdTag, size_t>;

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_IDS_H_
