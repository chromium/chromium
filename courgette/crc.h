// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_CRC_H_
#define COURGETTE_CRC_H_

#include <stddef.h>
#include <stdint.h>

namespace courgette {

// Calculates Crc of the given buffer by calling CRC method in LZMA SDK
//
uint32_t CalculateCrc(const uint8_t* buffer, size_t size);

}  // namespace courgette
#endif  // COURGETTE_CRC_H_
