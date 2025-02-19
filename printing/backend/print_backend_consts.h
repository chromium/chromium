// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_CONSTS_H_
#define PRINTING_BACKEND_PRINT_BACKEND_CONSTS_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kCUPSEnterprisePrinter[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kValueFalse[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kValueTrue[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kPrinterStatus[];
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kLocationTagName[];
#endif

#if BUILDFLAG(USE_CUPS)
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kDriverInfoTagName[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kDriverNameTagName[];

// CUPS destination option names.
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kCUPSOptDeviceUri[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kCUPSOptPrinterInfo[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kCUPSOptPrinterLocation[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kCUPSOptPrinterMakeAndModel[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kCUPSOptPrinterType[];
COMPONENT_EXPORT(PRINT_BACKEND) extern const char kCUPSOptPrinterUriSupported[];
#endif  // BUILDFLAG(USE_CUPS)

#endif  // PRINTING_BACKEND_PRINT_BACKEND_CONSTS_H_
