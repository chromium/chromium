// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_SETTINGS_CONVERSION_CHROMEOS_H_
#define PRINTING_PRINT_SETTINGS_CONVERSION_CHROMEOS_H_

#include <vector>

#include "base/values.h"
#include "printing/mojom/print.mojom.h"

// ChromeOS-specific print settings conversion functions.
namespace printing {

COMPONENT_EXPORT(PRINTING)
base::Value::List ConvertClientInfoToJobSetting(
    const std::vector<mojom::IppClientInfo>& client_info);

// Assumes that `client_info_job_setting` is valid.
COMPONENT_EXPORT(PRINTING)
std::vector<mojom::IppClientInfo> ConvertJobSettingToClientInfo(
    const base::Value::List& client_info_job_setting);

}  // namespace printing

#endif  // PRINTING_PRINT_SETTINGS_CONVERSION_CHROMEOS_H_
