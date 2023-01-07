// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CUPS IPP utility methods

#ifndef PRINTING_BACKEND_CUPS_IPP_UTILS_H_
#define PRINTING_BACKEND_CUPS_IPP_UTILS_H_

#include <memory>

#include "base/values.h"

namespace printing {

class CupsConnection;

// Creates a CUPS connection using `print_backend_settings`.
std::unique_ptr<CupsConnection> CreateConnection(
    const base::Value::Dict* print_backend_settings);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_UTILS_H_
