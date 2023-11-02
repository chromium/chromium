// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTER_QUERY_RESULT_H_
#define PRINTING_PRINTER_QUERY_RESULT_H_

#include "base/component_export.h"

namespace printing {

// Specifies query status codes.
// This enum is used to record UMA histogram values and should not be
// reordered. Please keep in sync with PrinterStatusQueryResult in
// src/tools/metrics/histograms/enums.xml.
enum class COMPONENT_EXPORT(PRINTING_BASE) PrinterQueryResult {
  kUnknownFailure = 0,      // catchall error
  kSuccess = 1,             // successful
  kUnreachable = 2,         // failed to reach the host
  kHostnameResolution = 3,  // unable to resolve IP address from hostname
  kMaxValue = kHostnameResolution
};

}  // namespace printing

#endif  // PRINTING_PRINTER_QUERY_RESULT_H_
