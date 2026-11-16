// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_INK_DICT_CONVERSIONS_H_
#define PDF_PDF_INK_DICT_CONVERSIONS_H_

#include "base/values.h"
#include "pdf/buildflags.h"
#include "third_party/skia/include/core/SkColor.h"

static_assert(BUILDFLAG(ENABLE_PDF_INK2), "ENABLE_PDF_INK2 not set to true");

namespace chrome_pdf {

// Extracts the "color" dictionary from `dict` and converts its "r", "g", and
// "b" integer entries (which must be in the range [0, 255]) into an `SkColor`.
SkColor GetColorFromDict(const base::DictValue& dict);

// Converts the RGB components of `color` into a dictionary with "r", "g", and
// "b" integer entries.
base::DictValue SkColorToDict(SkColor color);

}  // namespace chrome_pdf

#endif  // PDF_PDF_INK_DICT_CONVERSIONS_H_
