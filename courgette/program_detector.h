// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_PROGRAM_DETECTOR_H_
#define COURGETTE_PROGRAM_DETECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "courgette/courgette.h"

namespace courgette {

class Disassembler;

// Returns a new instance of Disassembler inherited class if binary data given
// in |buffer| and |length| match a known binary format, otherwise null.
std::unique_ptr<Disassembler> DetectDisassembler(const uint8_t* buffer,
                                                 size_t length);

// Detects the type of an executable file, and it's length. The length may be
// slightly smaller than some executables (like ELF), but will include all bytes
// the courgette algorithm has special benefit for.
// On success:
//   Fills in |type| and |detected_length|, and returns C_OK.
// On failure:
//   Fills in |type| with UNKNOWN, |detected_length| with 0, and returns
//   C_INPUT_NOT_RECOGNIZED.
Status DetectExecutableType(const uint8_t* buffer,
                            size_t length,
                            ExecutableType* type,
                            size_t* detected_length);

}  // namespace courgette

#endif  // COURGETTE_PROGRAM_DETECTOR_H_
