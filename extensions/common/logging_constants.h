// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_LOGGING_CONSTANTS_H_
#define EXTENSIONS_COMMON_LOGGING_CONSTANTS_H_

#include "base/logging.h"

// Separate from constants.h to avoid pulling base/logging.h into many files.

namespace extension_misc {

// The minimum severity of a log or error in order to report it to the browser.
inline constexpr logging::LogSeverity kMinimumSeverityToReportError =
    logging::LOGGING_WARNING;

}  // namespace extension_misc

#endif  // EXTENSIONS_COMMON_LOGGING_CONSTANTS_H_
