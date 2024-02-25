// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compute_pressure/pressure_source_index.h"

namespace blink {

wtf_size_t ToSourceIndex(V8PressureSource::Enum source) {
  wtf_size_t index = static_cast<wtf_size_t>(source);
  CHECK_LT(index, V8PressureSource::kEnumSize);
  return index;
}

}  // namespace blink
