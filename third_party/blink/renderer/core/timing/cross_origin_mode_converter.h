// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_CROSS_ORIGIN_MODE_CONVERTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_CROSS_ORIGIN_MODE_CONVERTER_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_cross_origin_mode.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"

namespace blink {

// Converts a CrossOriginAttributeValue to the CrossOriginMode enum exposed by
// the SpeculationMeasurement API (used by the PreloadData and PreconnectData
// dictionaries).
inline V8CrossOriginMode ToV8CrossOriginMode(
    CrossOriginAttributeValue crossorigin) {
  switch (crossorigin) {
    case kCrossOriginAttributeNotSet:
      return V8CrossOriginMode(V8CrossOriginMode::Enum::kNone);
    case kCrossOriginAttributeAnonymous:
      return V8CrossOriginMode(V8CrossOriginMode::Enum::kAnonymous);
    case kCrossOriginAttributeUseCredentials:
      return V8CrossOriginMode(V8CrossOriginMode::Enum::kUseCredentials);
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_CROSS_ORIGIN_MODE_CONVERTER_H_
