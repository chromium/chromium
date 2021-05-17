// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_CPUID_BASE_FREQUENCY_PARSER_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_CPUID_BASE_FREQUENCY_PARSER_H_

#include <stdint.h>

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"

namespace content {

// Parses the CPU's base frequency from the CPUID brand string.
//
// Returns -1 if reading failed for any reason. If successful, the returned
// frequency is guaranteed to be greater than zero.
//
// Some processors' brand strings don't include the base frequency. Some
// processors don't have brand strings altogether.
int64_t CONTENT_EXPORT
ParseBaseFrequencyFromCpuid(base::StringPiece brand_string);

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_CPUID_BASE_FREQUENCY_PARSER_H_
