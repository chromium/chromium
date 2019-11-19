// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTER_QUERY_RESULT_CHROMEOS_H_
#define PRINTING_PRINTER_QUERY_RESULT_CHROMEOS_H_

#include "printing/printing_export.h"

namespace printing {

// Specifies query status codes.
enum PRINTING_EXPORT PrinterQueryResult {
  UNKNOWN_FAILURE,  // catchall error
  SUCCESS,          // successful
  UNREACHABLE,      // failed to reach the host
};

}  // namespace printing

#endif  // PRINTING_PRINTER_QUERY_RESULT_CHROMEOS_H_
