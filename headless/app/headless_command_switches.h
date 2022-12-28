// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_APP_HEADLESS_COMMAND_SWITCHES_H_
#define HEADLESS_APP_HEADLESS_COMMAND_SWITCHES_H_

#include "headless/public/headless_export.h"

namespace headless::switches {

HEADLESS_EXPORT extern const char kDefaultBackgroundColor[];
HEADLESS_EXPORT extern const char kDumpDom[];
HEADLESS_EXPORT extern const char kPrintToPDF[];
HEADLESS_EXPORT extern const char kPrintToPDFNoHeader[];
HEADLESS_EXPORT extern const char kScreenshot[];
HEADLESS_EXPORT extern const char kTimeout[];
HEADLESS_EXPORT extern const char kVirtualTimeBudget[];

}  // namespace headless::switches

#endif  // HEADLESS_APP_HEADLESS_COMMAND_SWITCHES_H_
