// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_STROKE_ID_H_
#define PDF_PDF_INK_STROKE_ID_H_

#include <stddef.h>

#include "base/types/strong_alias.h"

namespace chrome_pdf {

using InkStrokeId = base::StrongAlias<class InkStrokeIdTag, size_t>;

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_STROKE_ID_H_
