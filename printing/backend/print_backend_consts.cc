// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constant defines used in the print backend code

#include "printing/backend/print_backend_consts.h"

// TODO(dhoss): Evaluate removing the strings used as keys for
// `PrinterBasicInfo.options` in favor of fields in PrinterBasicInfo.
const char kCUPSEnterprisePrinter[] = "cupsEnterprisePrinter";
const char kDriverInfoTagName[] = "system_driverinfo";
const char kDriverNameTagName[] = "printer-make-and-model";
const char kLocationTagName[] = "printer-location";
const char kValueFalse[] = "false";
const char kValueTrue[] = "true";
const char kPrinterStatus[] = "printerStatus";

// The following values must match those defined in CUPS.
const char kCUPSOptDeviceUri[] = "device-uri";
const char kCUPSOptPrinterInfo[] = "printer-info";
const char kCUPSOptPrinterLocation[] = "printer-location";
const char kCUPSOptPrinterMakeAndModel[] = "printer-make-and-model";
const char kCUPSOptPrinterState[] = "printer-state";
const char kCUPSOptPrinterType[] = "printer-type";
const char kCUPSOptPrinterUriSupported[] = "printer-uri-supported";
