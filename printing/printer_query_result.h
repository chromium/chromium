// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTER_QUERY_RESULT_H_
#define PRINTING_PRINTER_QUERY_RESULT_H_

#include "printing/printing_export.h"

namespace printing {

// Specifies query status codes.
// This enum is used to record UMA histogram values and should not be
// reordered. Please keep in sync with PrinterStatusQueryResult in
// src/tools/metrics/histograms/enums.xml.
enum class PRINTING_EXPORT PrinterQueryResult {
  kUnknownFailure = 0,  // catchall error
  kSuccess = 1,         // successful
  kUnreachable = 2,     // failed to reach the host
  kMaxValue = kUnreachable
};

}  // namespace printing

#endif  // PRINTING_PRINTER_QUERY_RESULT_H_
