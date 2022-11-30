// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_CRC32_H_
#define COMPONENTS_ZUCCHINI_CRC32_H_

#include <stdint.h>

namespace zucchini {

// Calculates CRC-32 of the given range [|first|, |last|).
uint32_t CalculateCrc32(const uint8_t* first, const uint8_t* last);

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_CRC32_H_
