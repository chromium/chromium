// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_SETTINGS_CONVERSION_H_
#define PRINTING_PRINT_SETTINGS_CONVERSION_H_

#include <memory>

#include "base/component_export.h"
#include "base/values.h"
#include "printing/page_range.h"

namespace printing {

class PrintSettings;

COMPONENT_EXPORT(PRINTING)
PageRanges GetPageRangesFromJobSettings(const base::Value::Dict& job_settings);

// Returns nullptr on failure.
COMPONENT_EXPORT(PRINTING)
std::unique_ptr<PrintSettings> PrintSettingsFromJobSettings(
    const base::Value::Dict& job_settings);

// Use for debug/test only, because output is not completely consistent with
// format of `PrintSettingsFromJobSettings` input.
COMPONENT_EXPORT(PRINTING)
base::Value::Dict PrintSettingsToJobSettingsDebug(
    const PrintSettings& settings);

}  // namespace printing

#endif  // PRINTING_PRINT_SETTINGS_CONVERSION_H_
