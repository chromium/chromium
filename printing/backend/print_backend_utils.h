// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_
#define PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "printing/backend/print_backend.h"

namespace printing {

enum class Unit {
  kInches,
  kMillimeters,
};

COMPONENT_EXPORT(PRINT_BACKEND)
PrinterSemanticCapsAndDefaults::Paper ParsePaper(base::StringPiece value);

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_UTILS_H_
