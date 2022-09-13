// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/printer_provider/printer_provider_print_job.h"

namespace extensions {

PrinterProviderPrintJob::PrinterProviderPrintJob() = default;

PrinterProviderPrintJob::PrinterProviderPrintJob(
    PrinterProviderPrintJob&& other) = default;

PrinterProviderPrintJob& PrinterProviderPrintJob::operator=(
    PrinterProviderPrintJob&& other) = default;

PrinterProviderPrintJob::~PrinterProviderPrintJob() = default;

}  // namespace extensions
