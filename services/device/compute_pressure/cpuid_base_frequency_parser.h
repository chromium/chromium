// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CPUID_BASE_FREQUENCY_PARSER_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CPUID_BASE_FREQUENCY_PARSER_H_

#include <stdint.h>

#include "base/strings/string_piece.h"

namespace device {

// Parses the CPU's base frequency from the CPUID brand string.
//
// Returns -1 if reading failed for any reason. If successful, the returned
// frequency is guaranteed to be greater than zero.
//
// Some processors' brand strings don't include the base frequency. Some
// processors don't have brand strings altogether.
int64_t ParseBaseFrequencyFromCpuid(base::StringPiece brand_string);

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPUID_BASE_FREQUENCY_PARSER_H_
