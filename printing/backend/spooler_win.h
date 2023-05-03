// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_SPOOLER_WIN_H_
#define PRINTING_BACKEND_SPOOLER_WIN_H_

#include "base/component_export.h"

namespace printing::internal {

enum class SpoolerServiceStatus {
  kUnknown,
  kNotRunning,
  kRunning,
};

// Determines if the Windows print spooler is running.
COMPONENT_EXPORT(PRINT_BACKEND) SpoolerServiceStatus IsSpoolerRunning();

}  // namespace printing::internal

#endif  // PRINTING_BACKEND_SPOOLER_WIN_H_
